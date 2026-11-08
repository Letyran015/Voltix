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

struct SpiceResult {
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
    static const QRegularExpression badChars("[^A-Za-z0-9_]");
    for (int c = 0; c < syms.size(); ++c) {
        SymbolItem *s = syms[c];
        const QString k = s->kind();
        if (k == "ground" || k == "label") continue;

        QString name = s->ref;
        name.remove(badChars);
        if (name.isEmpty()) name = QString("X%1").arg(c);
        if (!name.startsWith(s->prefix(), Qt::CaseInsensitive))
            name = s->prefix() + name;
        if (k == "opamp")
            name = "E" + name;                  // source commandée en SPICE

        QStringList n;
        for (int i = 0; i < s->pinCount(); ++i)
            n << nodeOf(c, i);

        auto v = [&](const QString &key, const QString &def) { return spiceValue(s, key, def); };

        if (k == "resistor")
            lines << QString("%1 %2 %3 %4").arg(name, n[0], n[1], v("R", "1k"));
        else if (k == "capacitor")
            lines << QString("%1 %2 %3 %4 IC=%5").arg(name, n[0], n[1], v("C", "100n"), v("V", "0"));
        else if (k == "inductor")
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
            models << QString(".model M_%1 D(Is=%2 N=%3)").arg(name, v("Is", "1e-15"), v("N", "1"));
        } else if (k == "npn" || k == "pnp") {
            lines << QString("%1 %2 %3 %4 M_%1").arg(name, n[1], n[0], n[2]);
            models << QString(".model M_%1 %2(Bf=%3 Is=%4)")
                          .arg(name, k == "npn" ? "NPN" : "PNP", v("Bf", "100"), v("Is", "1e-14"));
        } else if (k == "opamp") {
            lines << QString("%1 %2 0 %3 %4 %5").arg(name, n[2], n[0], n[1], v("A", "1e6"));
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
    r.netlist = r.body + ".control\nop\nprint all\n.endc\n.end\n";

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