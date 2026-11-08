#pragma once
#include "symbolitem.h"
#include "wireitem.h"
#include "core.h"

#include <QMap>
#include <QStringList>
#include <algorithm>

struct SpiceResult {
    QString netlist;
    QString legend;   // correspondance nœud -> broches
    QString error;
    QString body;         // netlist sans section .control
    QStringList nodes;    // nœuds SPICE hors masse
};

inline QString spiceValue(const SymbolItem *s, const QString &name, const QString &def)
{
    for (const Prop &p : s->props)
        if (p.name == name && !p.value.isEmpty())
            return p.value;
    return def;
}

inline SpiceResult buildNetlist(const QList<SymbolItem *> &symsIn, const QList<WireItem *> &wires)
{
    SpiceResult r;
    QList<SymbolItem *> syms = symsIn;
    std::sort(syms.begin(), syms.end(),
              [](SymbolItem *a, SymbolItem *b) { return a->index < b->index; });

    // 1. modèle C et calcul des nets
    Schema *sc = schema_new();
    for (int c = 0; c < syms.size(); ++c)
        for (int i = 0; i < syms[c]->pinCount(); ++i) {
            const QPointF p = syms[c]->pinScene(i);
            schema_add_pin(sc, c, i, qRound(p.x()), qRound(p.y()));
        }
    for (WireItem *w : wires) {
        const QPolygonF &pts = w->points();
        if (pts.size() < 2) continue;
        schema_add_wire(sc, qRound(pts.first().x()), qRound(pts.first().y()),
                        qRound(pts.last().x()),  qRound(pts.last().y()));
    }
    schema_compute_nets(sc);

    // 2. nœuds SPICE : les nets de masse deviennent "0"
    QMap<int, QString> node;
    QMap<int, QStringList> members;
    bool hasGnd = false;
    for (int c = 0; c < syms.size(); ++c)
        if (syms[c]->kind() == "ground") {
            const int n = schema_pin_net(sc, c, 0);
            if (n >= 0) { node[n] = "0"; hasGnd = true; }
        }

    int next = 1;
    QStringList floating;
    auto nodeOf = [&](int c, int p) -> QString {
        const int n = schema_pin_net(sc, c, p);
        if (n < 0) {
            floating << QString("%1.%2").arg(syms[c]->ref).arg(p + 1);
            return "?";
        }
        if (!node.contains(n))
            node[n] = QString("n%1").arg(next++);
        members[n] << QString("%1.%2").arg(syms[c]->ref).arg(p + 1);
        return node[n];
    };

    // 3. une ligne par composant
    QStringList lines, models;
    for (int c = 0; c < syms.size(); ++c) {
        SymbolItem *s = syms[c];
        const QString k = s->kind();
        if (k == "ground") continue;

        QString name = s->ref.trimmed().replace(' ', '_');
        if (!name.startsWith(s->prefix(), Qt::CaseInsensitive))
            name = s->prefix() + name;

        const QString a = nodeOf(c, 0);
        const QString b = nodeOf(c, 1);

        if (k == "resistor")
            lines << QString("%1 %2 %3 %4").arg(name, a, b, spiceValue(s, "R", "1k"));
        else if (k == "capacitor")
            lines << QString("%1 %2 %3 %4 IC=%5")
                         .arg(name, a, b, spiceValue(s, "C", "100n"), spiceValue(s, "V", "0"));
        else if (k == "vsource")
            lines << QString("%1 %2 %3 DC %4").arg(name, a, b, spiceValue(s, "U", "5"));
        else if (k == "diode") {
            lines << QString("%1 %2 %3 M_%1").arg(name, a, b);
            models << QString(".model M_%1 D(Is=%2 N=%3)")
                          .arg(name, spiceValue(s, "Is", "1e-15"), spiceValue(s, "N", "1"));
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
    for (auto it = node.cbegin(); it != node.cend(); ++it)
        if (it.value() != "0")
            r.nodes << it.value();

    for (auto it = node.cbegin(); it != node.cend(); ++it)
        r.legend += QString("%1 : %2\n").arg(it.value(), members[it.key()].join(", "));
    return r;
}
