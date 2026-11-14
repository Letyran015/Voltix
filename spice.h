#pragma once
#include "symbolitem.h"
#include "wireitem.h"
#include "core.h"

#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <algorithm>

struct SpiceMeter {
    QString ref;               // V M1, AM2...
    QString label;             // nom de la courbe
    QString expr;              // expression ngspice : v(n1)-v(n2) ou i(vam_AM1)
    QString unit;              // "V" ou "A"
    SymbolItem *sym = nullptr;
};

struct SpiceResult {
    QList<SpiceMeter> meters;         // instruments de mesure posés sur le schéma
    QString netlist;
    QString body;                     // netlist sans section .control
    QString legend;                   // correspondance nœud -> broches
    QStringList nodes;                // nœuds SPICE hors masse (reliés à un composant)
    QMap<QString, QString> nodeDesc;  // nœud -> "R1.2, C1.1"
    QStringList dcRefs;               // sources DC (noms affichés)
    QStringList dcNames;              // sources DC (noms SPICE)
    bool hasAc = false;               // au moins une source sinus avec AC 1
    QString error;
};


inline QString spiceValue(const SymbolItem *s, const QString &name, const QString &def)
{
    static const QRegularExpression ok("^[A-Za-z0-9.+_-]+$");
    for (const Prop &p : s->props) {
        if (p.name != name)
            continue;
        const QString v = p.value.trimmed();
        return ok.match(v).hasMatch() ? v : def;
    }
    return def;
}

// nom de nœud issu d'une étiquette, toujours sûr pour la netlist
inline QString labelName(const SymbolItem *s)
{
    static const QRegularExpression bad("[^A-Za-z0-9_]");
    QString nm;
    for (const Prop &p : s->props)
        if (p.name == "Nom")
            nm = p.value.trimmed();
    nm.remove(bad);
    if (nm.isEmpty() || nm[0].isDigit())
        nm = "n_" + nm;
    if (nm.compare("gnd", Qt::CaseInsensitive) == 0)
        nm += "_";                      // "gnd" est un alias de la masse en SPICE
    return nm;
}

// modèle C du schéma ; syms doit être trié par index
inline Schema *buildSchema(const QList<SymbolItem *> &syms, const QList<WireItem *> &wires)
{
    Schema *sc = schema_new();
    for (int c = 0; c < syms.size(); ++c)
        for (int i = 0; i < syms[c]->pinCount(); ++i) {
            const QPointF p = syms[c]->pinScene(i);
            schema_add_pin(sc, c, i, qRound(p.x()), qRound(p.y()));
        }
    for (WireItem *w : wires) {
        const QPolygonF &pts = w->points();
        if (pts.size() < 2) continue;
        QVector<int> xy;
        for (const QPointF &p : pts)
            xy << qRound(p.x()) << qRound(p.y());
        schema_add_wire(sc, xy.constData(), int(pts.size()));
    }
    schema_compute_nets(sc);
    return sc;
}

inline SpiceResult buildNetlist(const QList<SymbolItem *> &symsIn, const QList<WireItem *> &wires,
                                bool ac = false)
{
    SpiceResult r;
    QList<SymbolItem *> syms = symsIn;
    std::sort(syms.begin(), syms.end(),
              [](SymbolItem *a, SymbolItem *b) { return a->index < b->index; });

    Schema *sc = buildSchema(syms, wires);

    QMap<int, QString> node;
    QMap<int, QStringList> members;
    QSet<QString> used;                 // noms déjà pris (en minuscules)
    used << "0";
    bool hasGnd = false;

    // 1. masse = nœud 0
    for (int c = 0; c < syms.size(); ++c)
        if (syms[c]->kind() == "ground") {
            const int n = schema_pin_net(sc, c, 0);
            if (n >= 0) { node[n] = "0"; hasGnd = true; }
        }

    // 2. étiquettes : elles nomment leur net (la première gagne)
    for (int c = 0; c < syms.size(); ++c) {
        if (syms[c]->kind() != "label") continue;
        const int n = schema_pin_net(sc, c, 0);
        if (n < 0 || node.contains(n)) continue;
        const QString base = labelName(syms[c]);
        QString nm = base;
        int k = 2;
        while (used.contains(nm.toLower()))
            nm = base + "_" + QString::number(k++);
        used << nm.toLower();
        node[n] = nm;
    }

    int next = 1;
    QStringList floating;
    auto nodeOf = [&](int c, int p) -> QString {
        const int n = schema_pin_net(sc, c, p);
        if (n < 0) {
            floating << QString("%1.%2").arg(syms[c]->ref).arg(p + 1);
            return "?";
        }
        if (!node.contains(n)) {
            QString nm;
            do { nm = QString("n%1").arg(next++); } while (used.contains(nm));
            used << nm;
            node[n] = nm;
        }
        members[n] << QString("%1.%2").arg(syms[c]->ref).arg(p + 1);
        return node[n];
    };

    QStringList lines, models;
    QSet<QString> subDone;
    static const QRegularExpression badChars("[^A-Za-z0-9_]");
    for (int c = 0; c < syms.size(); ++c) {
        SymbolItem *s = syms[c];
        const QString k = s->kind();
        static const QSet<QString> simulated = {"resistor", "capacitor", "inductor", "vsource",
                                                "vsin", "vpulse", "diode", "npn", "pnp", "opamp",
                                                "transformer", "vmeter", "ammeter",
                                                "zener", "led", "schottky", "nmos", "pmos", "njfet", "pjfet",
                                                "thyristor", "triac", "diac", "vcvs", "vccs", "cccs", "ccvs",
                                                "isource", "pot", "ntc", "bridge", "igbt",
                                                "timer555", "tl431", "opto", "vswitch", "summer", "gain", "comparator",
                                                "triangle", "quartz", "speaker", "photodiode", "tunnel",
                                                "lsrc", "clock", "lprobe",
                                                "rheostat", "ldr", "cap_pol", "cap_var", "ind_fe", "ind_var",
                                                "transformer_ip", "transformer_op", "bell", "bulb"};
        if (!simulated.contains(k)) continue;      // masse, étiquettes, électrotechnique

        if (k == "vmeter" || k == "ammeter" || k == "lprobe") {     // instruments : ne perturbent pas le circuit
            QString id = s->ref;
            id.remove(badChars);
            if (id.isEmpty()) id = QString::number(c);
            const QString a = nodeOf(c, 0), b = (k == "lprobe") ? QString("0") : nodeOf(c, 1);
            SpiceMeter m;
            m.ref = s->ref;
            m.sym = s;
            QString nom = s->props.isEmpty() ? QString() : s->props[0].value.trimmed();
            nom.remove(badChars);
            m.label = nom.isEmpty() ? s->ref : nom;
            if (k == "ammeter") {
                const QString nm = "Vam_" + id;
                lines << QString("%1 %2 %3 DC 0").arg(nm, a, b);   // source de 0 V : sert d'ampèremètre
                m.expr = QString("i(%1)").arg(nm);
                m.unit = "A";
            } else {
                lines << QString("Rvm_%1 %2 %3 1G").arg(id, a, b);  // 1 GΩ : ne charge rien, mais crée les nœuds
                if (a == b)        m.expr = "0";
                else if (b == "0") m.expr = QString("v(%1)").arg(a);       // v(0) n'existe pas en ngspice
                else if (a == "0") m.expr = QString("-v(%1)").arg(b);
                else               m.expr = QString("v(%1)-v(%2)").arg(a, b);
                m.unit = "V";
            }
            r.meters << m;
            continue;
        }

        QString name = s->ref;
        name.remove(badChars);
        if (name.isEmpty()) name = QString("X%1").arg(c);
        if (!name.startsWith(s->prefix(), Qt::CaseInsensitive))
            name = s->prefix() + name;
        if (k == "opamp") {
            double um = 0;
            for (const Prop &pp : s->props)
                if (pp.name == "Umax") um = pp.value.trimmed().toDouble();
            name = (um > 0 ? "B" : "E") + name; // B = source comportementale (saturation), E = source commandée
        }
        if (k == "igbt")
            name = "S" + name;                  // interrupteur commandé en SPICE
        if (k == "led")
            name = "D" + name;                  // en SPICE, L = inductance
        if (k == "ldr" || k == "bell" || k == "bulb")
            name = "R" + name;                  // charges resistives
        if (k == "thyristor" || k == "triac" || k == "diac")
            name = "X" + name;                  // sous-circuit

        QStringList n;
        for (int i = 0; i < s->pinCount(); ++i)
            n << nodeOf(c, i);

        auto v = [&](const QString &key, const QString &def) { return spiceValue(s, key, def); };
        auto vn = [](const QString &nd) { return nd == "0" ? QString("0") : QString("v(%1)").arg(nd); };
        auto ve = [&](const QString &a, const QString &b) { return QString("(%1-%2)").arg(vn(a), vn(b)); };
        // parametres de modele facultatifs : emis seulement s'ils sont non nuls
        auto optParams = [&](const QStringList &keys) {
            QString out;
            for (const QString &key : keys) {
                const QString val = v(key, "0");
                bool ok = false;
                const double d = val.toDouble(&ok);
                if (val.isEmpty() || (ok && d == 0.0)) continue;
                if (key == "Nf" && ok && d == 1.0) continue;
                out += " " + key + "=" + val;
            }
            return out;
        };

        if (k == "resistor" || k == "rheostat" || k == "bell" || k == "bulb")
            lines << QString("%1 %2 %3 %4").arg(name, n[0], n[1], v("R", k == "bell" ? "8" : (k == "bulb" ? "50" : "1k")));
        else if (k == "ldr")
            lines << QString("%1 %2 %3 {(%4)*(10/max(%5,0.01))^(%6)}").arg(name, n[0], n[1], v("R10", "10k"), v("Lux", "100"), v("Gamma", "0.7"));
        else if (k == "cap_pol") {
            bool oke = false;
            const double esr = v("ESR", "0").toDouble(&oke);
            if (oke && esr > 0) {
                lines << QString("%1 %2 %1_x %3 IC=%4").arg(name, n[0], v("C", "100u"), v("V", "0"));
                lines << QString("R%1e %1_x %2 %3").arg(name, n[1], v("ESR", "0"));
            } else {
                lines << QString("%1 %2 %3 %4 IC=%5").arg(name, n[0], n[1], v("C", "100u"), v("V", "0"));
            }
        }
        else if (k == "capacitor" || k == "cap_var")
            lines << QString("%1 %2 %3 %4 IC=%5").arg(name, n[0], n[1], v("C", "100n"), v("V", "0"));
        else if (k == "transformer" || k == "transformer_ip" || k == "transformer_op") {
            lines << QString("L%1p %2 %3 %4").arg(name, n[0], n[1], v("Lp", "10m"));
            if (k == "transformer_op")      // secondaire inverse : le point est sur la borne du bas
                lines << QString("L%1s %2 %3 %4").arg(name, n[3], n[2], v("Ls", "10m"));
            else
                lines << QString("L%1s %2 %3 %4").arg(name, n[2], n[3], v("Ls", "10m"));
            lines << QString("K%1 L%1p L%1s %2").arg(name, v("k", "0.99"));
        }
        else if (k == "inductor" || k == "ind_fe" || k == "ind_var")
            lines << QString("%1 %2 %3 %4 IC=%5").arg(name, n[0], n[1], v("L", "1m"), v("Ic", "0"));
        else if (k == "vsource") {
            lines << QString("%1 %2 %3 DC %4").arg(name, n[0], n[1], v("U", "5"));
            r.dcRefs << s->ref;
            r.dcNames << name;
        } else if (k == "vsin") {
            lines << QString("%1 %2 %3 SIN(%4 %5 %6)%7")
            .arg(name, n[0], n[1], v("Voff", "0"), v("Vamp", "1"), v("Freq", "1k"),
                 ac ? QString(" AC 1") : QString());
            if (ac) r.hasAc = true;
        } else if (k == "vpulse")
            lines << QString("%1 %2 %3 PULSE(%4)").arg(name, n[0], n[1],
                                                       QStringList{v("V1", "0"), v("V2", "5"), v("Td", "0"), v("Tr", "1n"),
                                                                   v("Tf", "1n"), v("Pw", "500u"), v("Per", "1m")}.join(' '));
        else if (k == "diode") {
            lines << QString("%1 %2 %3 M_%1").arg(name, n[0], n[1]);
            models << QString(".model M_%1 D(Is=%2 N=%3%4)").arg(name, v("Is", "1e-15"), v("N", "1"),
                          optParams({"Rs", "Cjo", "Tt", "Bv"}));
        } else if (k == "npn" || k == "pnp") {
            lines << QString("%1 %2 %3 %4 M_%1").arg(name, n[1], n[0], n[2]);
            models << QString(".model M_%1 %2(Bf=%3 Is=%4%5)")
                          .arg(name, k == "npn" ? "NPN" : "PNP", v("Bf", "100"), v("Is", "1e-14"),
                               optParams({"Nf", "Vaf", "Ikf", "Rb", "Rc", "Re"}));
        } else if (k == "zener" || k == "led" || k == "schottky") {
            lines << QString("%1 %2 %3 M_%1").arg(name, n[0], n[1]);
            models << QString(".model M_%1 D(Is=%2 N=%3%4%5)").arg(name, v("Is", "1e-15"), v("N", "1"),
                          optParams({"Rs", "Bv"}), k == "zener" ? QString(" Ibv=5m") : QString());
        } else if (k == "nmos" || k == "pmos") {
            lines << QString("%1 %2 %3 %4 %4 M_%1").arg(name, n[1], n[0], n[2]);
            models << QString(".model M_%1 %2(Level=1 Vto=%3 Kp=%4%5)")
                          .arg(name, k == "nmos" ? "NMOS" : "PMOS", v("Vto", k == "nmos" ? "2" : "-2"),
                               v("Kp", "0.5"), optParams({"Lambda"}));
        } else if (k == "njfet" || k == "pjfet") {
            lines << QString("%1 %2 %3 %4 M_%1").arg(name, n[1], n[0], n[2]);
            models << QString(".model M_%1 %2(Vto=%3 Beta=%4%5)")
                          .arg(name, k == "njfet" ? "NJF" : "PJF", v("Vto", k == "njfet" ? "-2" : "2"),
                               v("Beta", "1m"), optParams({"Lambda", "Rd", "Rs"}));
        } else if (k == "thyristor" || k == "triac") {
            const QString sc = "SCR_" + name;
            models << QString(".model MN_%1 NPN(Bf=%2 Is=1e-14)").arg(name, v("Bf", "50"));
            models << QString(".model MP_%1 PNP(Bf=%2 Is=1e-14)").arg(name, v("Bf", "50"));
            models << QString(".subckt %1 a k g\nQ1 b2 g k MN_%2\nQ2 g b2 a MP_%2\nRgk g k 1k\n.ends").arg(sc, name);
            if (k == "thyristor") {
                lines << QString("%1 %2 %3 %4 %5").arg(name, n[0], n[1], n[2], sc);
            } else {
                const QString tr = "TRIAC_" + name;
                models << QString(".subckt %1 mt2 mt1 g\nVgs g gi 0\nRg gi mt1 10\nXa mt2 mt1 ga %2\n"
                                  "Xb mt1 mt2 gb %2\nBa mt1 ga I=abs(i(Vgs))\nBb mt2 gb I=abs(i(Vgs))\n.ends")
                              .arg(tr, sc);
                lines << QString("%1 %2 %3 %4 %5").arg(name, n[0], n[1], n[2], tr);
            }
        } else if (k == "diac") {
            lines << QString("D%1a %2 %1m M_%1").arg(name, n[0]);
            lines << QString("D%1b %2 %1m M_%1").arg(name, n[1]);
            models << QString(".model M_%1 D(Is=1e-14 Bv=%2 Ibv=1m)").arg(name, v("Vbo", "32"));
        } else if (k == "vcvs" || k == "vccs") {
            lines << QString("%1 %2 %3 %4 %5 %6").arg(name, n[2], n[3], n[0], n[1], v("G", "1"));
        } else if (k == "cccs" || k == "ccvs") {
            lines << QString("Vsense_%1 %2 %3 DC 0").arg(name, n[0], n[1]);
            lines << QString("%1 %2 %3 Vsense_%1 %4").arg(name, n[2], n[3], v("G", "1"));
        } else if (k == "isource") {
            lines << QString("%1 %2 %3 DC %4").arg(name, n[1], n[0], v("I", "10m"));
        } else if (k == "pot") {
            lines << QString("%1a %2 %3 {(%4)*max(%5,1e-6)}").arg(name, n[0], n[2], v("R", "10k"), v("Pos", "0.5"));
            lines << QString("%1b %2 %3 {(%4)*max(1-(%5),1e-6)}").arg(name, n[2], n[1], v("R", "10k"), v("Pos", "0.5"));
        } else if (k == "ntc") {
            lines << QString("%1 %2 %3 {(%4)*exp((%5)*(1/((%6)+273.15)-1/298.15))}")
                         .arg(name, n[0], n[1], v("R25", "10k"), v("B", "3950"), v("T", "25"));
        } else if (k == "lsrc") {
            bool ok = false;
            const double st = v("State", "1").toDouble(&ok);
            const QString vh = v("Vh", "5");
            lines << QString("V%1 %2 0 DC %3").arg(name, n[0], (ok && st > 0.5) ? vh : QString("0"));
        } else if (k == "clock") {
            const QString f = v("Freq", "1k"), vh = v("Vh", "5");
            lines << QString("V%1 %2 0 PULSE(0 %3 0 1n 1n {0.5/%4} {1/%4})").arg(name, n[0], vh, f);
        } else if (k == "timer555") {
            if (!subDone.contains("NE555")) {
                subDone << "NE555";
                models << ".subckt NE555 gnd trig out rst ctrl thr dis vcc\n"
                          "Rd1 vcc ctrl 5k\nRd2 ctrl nm 5k\nRd3 nm gnd 5k\n"
                          "Rt trig gnd 1G\nRh thr gnd 1G\nRr rst gnd 1G\n"
                          "Cq q gnd 100p\nRq q gnd 1T\n"
                          "Bq gnd q I=10m*(((v(trig,gnd)<v(nm,gnd))&&(v(thr,gnd)<=v(ctrl,gnd))&&(v(rst,gnd)>=0.7))*(1-v(q,gnd))"
                          "+((v(thr,gnd)>v(ctrl,gnd))||(v(rst,gnd)<0.7))*(0-v(q,gnd)))\n"
                          "Bo oi gnd V=(v(vcc,gnd)-1.7)*(v(q,gnd)>0.5)+0.1*(v(q,gnd)<=0.5)\n"
                          "Ro oi out 10\n"
                          "Sd dis gnd gnd q SWD\n"
                          ".model SWD SW(Vt=-0.5 Vh=0.1 Ron=10 Roff=1G)\n.ends";
            }
            lines << QString("X%1 %2 %3 %4 %5 %6 %7 %8 %9 NE555").arg(name, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7]);
        } else if (k == "tl431") {
            lines << QString("B%1 %2 %3 I=0.1*tanh(1000*max(0,%4-2.495))*tanh(max(0,%5)/0.5)")
                         .arg(name, n[1], n[2], ve(n[0], n[2]), ve(n[1], n[2]));
            lines << QString("D%1 %2 %3 MD_%1").arg(name, n[2], n[1]);
            lines << QString("R%1r %2 %3 1G").arg(name, n[0], n[2]);
            models << QString(".model MD_%1 D(Is=1e-12)").arg(name);
        } else if (k == "opto") {
            bool okc = false;
            const double ctr = v("CTR", "1").toDouble(&okc);
            lines << QString("V%1s %2 %1_a DC 0").arg(name, n[0]);
            lines << QString("D%1 %1_a %2 MD_%1").arg(name, n[1]);
            lines << QString("F%1 %2 %1_b V%1s %3").arg(name, n[3], QString::number((okc ? ctr : 1.0) / 300.0, 'g', 6));
            lines << QString("Q%1 %2 %1_b %3 MQ_%1").arg(name, n[2], n[3]);
            models << QString(".model MD_%1 D(Is=1e-14 N=1.8)").arg(name);
            models << QString(".model MQ_%1 NPN(Bf=300 Is=1e-14)").arg(name);
        } else if (k == "vswitch") {
            lines << QString("%1 %2 %3 %4 %5 MS_%1").arg(name, n[2], n[3], n[0], n[1]);
            models << QString(".model MS_%1 SW(Vt=%2 Vh=0.1 Ron=%3 Roff=%4)").arg(name, v("Vt", "2.5"), v("Ron", "10m"), v("Roff", "100meg"));
        } else if (k == "summer") {
            lines << QString("B%1 %2 0 V=(%3)*%4+(%5)*%6").arg(name, n[2], v("G1", "1"), vn(n[0]), v("G2", "1"), vn(n[1]));
        } else if (k == "gain") {
            lines << QString("B%1 %2 0 V=(%3)*%4").arg(name, n[1], v("K", "1"), vn(n[0]));
        } else if (k == "comparator") {
            lines << QString("B%1 %2 0 V=(%3)+((%4)-(%3))*0.5*(1+tanh(10000*%5))")
                         .arg(name, n[2], v("Vl", "0"), v("Vh", "5"), ve(n[0], n[1]));
        } else if (k == "triangle") {
            lines << QString("%1 %2 %3 PULSE({%4} {%5} 0 {0.499/%6} {0.499/%6} {0.002/%6} {1/%6})")
                         .arg(name, n[0], n[1], v("Vmin", "0"), v("Vmax", "5"), v("Freq", "10k"));
        } else if (k == "quartz") {
            const QString f = v("freq", "1meg"), rs = v("Rs", "540"), q = v("Q", "1000");
            lines << QString("L%1s %2 %1_1 {(%3)*(%4)/(6.283185307*(%5))}").arg(name, n[0], q, rs, f);
            lines << QString("C%1s %1_1 %1_2 {1/(6.283185307*(%2)*(%3)*(%4))}").arg(name, f, q, rs);
            lines << QString("R%1s %1_2 %2 %3").arg(name, n[1], rs);
            lines << QString("C%1p %2 %3 %4").arg(name, n[0], n[1], v("Cp", "13p"));
        } else if (k == "speaker") {
            const QString fs = v("fs", "35"), qm = v("Qms", "2.8"), res = v("Res", "32");
            lines << QString("R%1e %2 %1_x %3").arg(name, n[0], v("Re", "5"));
            lines << QString("R%1r %1_x %2 %3").arg(name, n[1], res);
            lines << QString("L%1m %1_x %2 {(%3)/(6.283185307*(%4)*(%5))}").arg(name, n[1], res, fs, qm);
            lines << QString("C%1m %1_x %2 {(%3)/(6.283185307*(%4)*(%5))}").arg(name, n[1], qm, fs, res);
        } else if (k == "photodiode") {
            lines << QString("D%1 %2 %3 M_%1").arg(name, n[0], n[1]);
            models << QString(".model M_%1 D(Is=%2 N=%3%4)").arg(name, v("Is", "1e-15"), v("N", "1"), optParams({"Rs"}));
            lines << QString("I%1p %2 %3 DC %4").arg(name, n[1], n[0], v("Iph", "0"));
        } else if (k == "tunnel") {
            const QString vd = ve(n[0], n[1]);
            const QString ip = v("Ip", "1m"), vp = v("Vp", "0.1");
            lines << QString("B%1 %2 %3 I=(%4)*(%5/(%6))*exp(min(1-%5/(%6),50))+1e-12*(exp(min(%5/0.0259,60))-1)+1e-9*%5")
                         .arg(name, n[0], n[1], ip, vd, vp);
        } else if (k == "opamp") {
            const QString um = v("Umax", "0");
            bool okU = false;
            const double umv = um.toDouble(&okU);
            if (okU && umv > 0) {
                auto vv = [](const QString &nd) { return nd == "0" ? QString("0") : QString("v(%1)").arg(nd); };
                lines << QString("%1 %2 0 V=%3*tanh((%4)*(%5-%6)/%3)")
                             .arg(name, n[2], um, v("A", "1e6"), vv(n[0]), vv(n[1]));
            } else {
                lines << QString("%1 %2 0 %3 %4 %5").arg(name, n[2], n[0], n[1], v("A", "1e6"));
            }
        } else if (k == "bridge") {
            lines << QString("D%1a %2 %3 M_%1").arg(name, n[0], n[2]);
            lines << QString("D%1b %2 %3 M_%1").arg(name, n[1], n[2]);
            lines << QString("D%1c %2 %3 M_%1").arg(name, n[3], n[0]);
            lines << QString("D%1d %2 %3 M_%1").arg(name, n[3], n[1]);
            models << QString(".model M_%1 D(Is=%2 N=%3%4%5)").arg(name, v("Is", "76.9p"), v("N", "1.45"),
                                                                   optParams({"Rs"}), optParams({"Bv"}));
        } else if (k == "igbt") {
            lines << QString("%1 %2 %3 %4 0 MS_%1").arg(name, n[1], n[2], n[0]);   // commande referencee a la masse (comme PSIM)
            lines << QString("D%1 %2 %3 MD_%1").arg(name, n[2], n[1]);
            models << QString(".model MS_%1 SW(Vt=%2 Vh=0.2 Ron=%3 Roff=1e8)").arg(name, v("Vth", "5"), v("Ron", "50m"));
            models << QString(".model MD_%1 D(Is=1e-12 N=1 Rs=10m)").arg(name);
        }
    }
    schema_free(sc);

    if (!hasGnd) {
        r.error = "Le circuit n'a pas de masse reliée : ajoutez une masse et reliez-la par un fil.";
        return r;
    }
    if (!floating.isEmpty()) {
        r.error = "Broches non connectées : " + floating.join(", ");
        return r;
    }

    r.body = "* Voltix\n" + lines.join("\n") + "\n" + models.join("\n") + "\n";
    QString opCtl = "op\nprint all\n";
    for (int i = 0; i < r.meters.size(); ++i)
        opCtl += QString("let m%1 = %2\nprint m%1\n").arg(i).arg(r.meters[i].expr);
    r.netlist = r.body + ".control\n" + opCtl + ".endc\n.end\n";

    for (auto it = node.cbegin(); it != node.cend(); ++it) {
        if (!members.contains(it.key()))
            continue;                               // nœud sans composant (étiquette seule)
        const QString desc = members[it.key()].join(", ");
        r.legend += QString("%1 : %2\n").arg(it.value(), desc);
        if (it.value() != "0") {
            r.nodes << it.value();
            r.nodeDesc[it.value()] = desc;
        }
    }
    return r;
}