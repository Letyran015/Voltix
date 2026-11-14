// Voltix - bibliothèque d'électrotechnique pilotée par des données (elib_data.h).
// Chaque symbole est décrit par : identifiant, nom, dossier, préfixe de repère, broches,
// liste de tracés et propriétés. Les dessins sont originaux (formes normalisées CEI 60617).
//
// Tracés (séparés par « ; ») :
//   L x1 y1 x2 y2        trait                    D  x1 y1 x2 y2   trait pointillé
//   DD x1 y1 x2 y2       trait mixte              P / DP x y ...   ligne brisée (pleine / pointillée)
//   Q x y ...            polygone                 K x y ...        polygone plein
//   R x y w h            rectangle                RF x y w h       rectangle plein
//   RR x y w h           rectangle à bouts ronds  C x y r          cercle
//   F x y r              disque plein             E x y r #rrggbb  disque coloré
//   A x y w h a0 da      arc (degrés)             W e              épaisseur du trait
//   O #rrggbb | O -      couleur du trait         T/B/I/TL/TR x y px texte   (centré, gras, italique, gauche, droite)
// Dans un texte : « _ » = espace, « $b0 » = 1re partie de la propriété Bornes, « $Nom » = valeur d'une propriété.
#pragma once
#include "elib_spec.h"
#include "elib_data.h"
#include "symbolitem.h"

#include <QFont>
#include <QHash>
#include <QPainter>
#include <QPolygonF>
#include <QStringList>
#include <QStyleOptionGraphicsItem>
#include <QTransform>
#include <QVector>

inline const LibSpec *libFind(const QString &id)
{
    static QHash<QString, const LibSpec *> map;
    if (map.isEmpty())
        for (int i = 0; i < kLibCount; ++i)
            map.insert(QString::fromUtf8(kLib[i].id), &kLib[i]);
    return map.value(id, nullptr);
}

// Rôle d'un symbole pour la vérification du projet (bobine, contact, pôles de puissance...)
inline QString electroRole(const QString &kind)
{
    if (const LibSpec *s = libFind(kind))
        return QString::fromUtf8(s->role);
    if (kind == "coil" || kind == "relay" || kind == "timer_coil") return "coil";
    if (kind == "thermal_nc") return "thcontact";
    if (kind == "contact_no" || kind == "contact_nc" || kind == "contact_co"
        || kind == "contact_td_no" || kind == "contact_td_nc") return "contact";
    if (kind.startsWith("contactor")) return "power";
    if (kind == "thermal3") return "thermal";
    return QString();
}

class LibItem : public SymbolItem
{
public:
    explicit LibItem(const LibSpec &s) : m_s(s), m_role(QString::fromUtf8(s.role))
    {
        for (const QString &pp : QString::fromUtf8(s.pins).split(';', Qt::SkipEmptyParts)) {
            const QStringList xy = pp.split(',');
            if (xy.size() == 2)
                m_pins << QPointF(xy[0].toDouble(), xy[1].toDouble());
        }
        for (const QString &pr : QString::fromUtf8(s.props).split(';', Qt::SkipEmptyParts)) {
            const QStringList f = pr.split('|');
            if (f.size() >= 5)
                props.append({f[0], f[1], f[2], f[3], f[4] == "1"});
        }
        if (m_role == "contact" || m_role == "thcontact" || m_role == "power")
            showRef = false;                       // le repère affiché est celui de la bobine associée

        for (const QString &o : QString::fromUtf8(s.ops).split(';', Qt::SkipEmptyParts)) {
            const QStringList t = o.trimmed().split(' ', Qt::SkipEmptyParts);
            if (t.isEmpty())
                continue;
            Op op;
            op.t = t[0];
            const bool text = isText(op.t);
            for (int i = 1; i < t.size(); ++i) {
                if (text && i >= 4) break;
                op.v << t[i].toDouble();
            }
            if (text) op.s = t.mid(4).join(' ');
            else if (op.t == "E") op.s = t.value(4);
            else if (op.t == "O") op.s = t.value(1);
            m_ops << op;
        }

        // zone occupée : tracés + broches
        QRectF r;
        auto add = [&r](const QRectF &q) { r = r.isNull() ? q : r.united(q); };
        for (const QPointF &p : m_pins)
            add(QRectF(p.x() - 4, p.y() - 4, 8, 8));
        for (const Op &op : m_ops) {
            auto n = [&op](int i) { return op.v.value(i); };
            if (op.t == "L" || op.t == "D" || op.t == "DD") {
                add(QRectF(QPointF(n(0), n(1)), QPointF(n(2), n(3))).normalized().adjusted(-1, -1, 1, 1));
            } else if (op.t == "R" || op.t == "RF" || op.t == "RR" || op.t == "A") {
                add(QRectF(n(0), n(1), n(2), n(3)));
            } else if (op.t == "C" || op.t == "F" || op.t == "E") {
                add(QRectF(n(0) - n(2), n(1) - n(2), 2 * n(2), 2 * n(2)));
            } else if (op.t == "P" || op.t == "DP" || op.t == "Q" || op.t == "K") {
                for (int i = 0; i + 1 < op.v.size(); i += 2)
                    add(QRectF(op.v[i] - 1, op.v[i + 1] - 1, 2, 2));
            } else if (isText(op.t)) {
                const qreal w = 0.62 * n(2) * qMax(1, int(subst(op.s).size()));
                qreal x0 = n(0) - w / 2;
                if (op.t == "TL") x0 = n(0);
                if (op.t == "TR") x0 = n(0) - w;
                add(QRectF(x0, n(1) - n(2) * 0.6, w, n(2) * 1.2));
            }
        }
        m_rect = r.adjusted(-5, -5, 5, 5);
    }

    QString prefix() const override { return QString::fromUtf8(m_s.prefix); }
    QString kind() const override { return QString::fromUtf8(m_s.id); }
    QString title() const override { return QString::fromUtf8(m_s.name); }
    int pinCount() const override { return m_pins.size(); }
    QPointF pinLocal(int i) const override { return m_pins.value(i); }

    QString role() const { return m_role; }
    QString folder() const { return QString::fromUtf8(m_s.group); }
    bool isSlave() const { return m_role == "contact" || m_role == "thcontact" || m_role == "power"; }
    bool isMaster() const { return m_role == "coil" || m_role == "thermal"; }
    QRectF body() const { return m_rect; }

    // Champs affichés près du symbole : Nom, Valeur, Champ 3, Champ 4 (ceux dont « Vis. » est coché).
    // Symbole esclave (contact, pôles) : le nom est celui du maître, la valeur est sa référence croisée.
    QStringList labelLines() const override
    {
        QStringList lines;
        if (isSlave()) {
            for (const Prop &pr : props) {
                if (!pr.show)
                    continue;
                if (pr.name == "Bobine" && !pr.value.trimmed().isEmpty())
                    lines << pr.value.trimmed();
                else if (pr.name == "Valeur" && !(xref.isEmpty() ? pr.value.trimmed() : xref).isEmpty())
                    lines << (xref.isEmpty() ? pr.value.trimmed() : xref);
                else if ((pr.name == "Champ 3" || pr.name == "Champ 4") && !pr.value.trimmed().isEmpty())
                    lines << pr.value.trimmed();
            }
            return lines;
        }
        if (showRef && !ref.isEmpty())
            lines << ref;
        for (const Prop &pr : props)
            if (pr.show && !pr.value.trimmed().isEmpty()
                && (pr.name == "Valeur" || pr.name == "Champ 3" || pr.name == "Champ 4"))
                lines << pr.value.trimmed();
        return lines;
    }

    void paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w) override
    {
        SymbolItem::paint(p, o, w);
        if (!isMaster() || (xrefNo.isEmpty() && xrefNc.isEmpty()))
            return;

        // Maître : tableau des références croisées « NO | NC » sous le symbole
        QTransform t;
        t.rotate(rotation());
        const QRectF rb = t.mapRect(bodyRect());
        qreal top = rb.bottom() + 4;
        if (rb.height() <= rb.width())
            top += 15.0 * labelLines().size();          // le texte est déjà sous le symbole
        const int rows = qMax(xrefNo.size(), xrefNc.size());
        const qreal rh = 12, half = 62;

        p->save();
        p->rotate(-rotation());
        QFont f = p->font();
        f.setPixelSize(10);
        p->setFont(f);
        p->setPen(QPen(isSelected() ? QColor(0, 120, 215) : QColor(Qt::black), 1));
        p->drawText(QRectF(-half, top, half - 5, rh), Qt::AlignRight | Qt::AlignVCenter, "NO");
        p->drawText(QRectF(5, top, half - 5, rh), Qt::AlignLeft | Qt::AlignVCenter, "NC");
        p->drawLine(QPointF(-half, top + rh), QPointF(half, top + rh));
        p->drawLine(QPointF(0, top), QPointF(0, top + rh * (rows + 1) + 2));
        for (int i = 0; i < xrefNo.size(); ++i)
            p->drawText(QRectF(-half - 30, top + rh * (i + 1) + 1, half + 25, rh), Qt::AlignRight | Qt::AlignVCenter, xrefNo[i]);
        for (int i = 0; i < xrefNc.size(); ++i)
            p->drawText(QRectF(5, top + rh * (i + 1) + 1, half + 25, rh), Qt::AlignLeft | Qt::AlignVCenter, xrefNc[i]);
        p->restore();
    }

protected:
    QRectF bodyRect() const override { return m_rect; }

    void drawBody(QPainter *p) const override
    {
        const QPen base = p->pen();
        QPen pen = base;
        for (const Op &op : m_ops) {
            auto n = [&op](int i) { return op.v.value(i); };
            if (op.t == "W") {
                pen.setWidthF(n(0));
                p->setPen(pen);
            } else if (op.t == "O") {
                pen.setColor(op.s == "-" || isSelected() ? base.color() : QColor(op.s));
                p->setPen(pen);
            } else if (op.t == "L") {
                p->drawLine(QPointF(n(0), n(1)), QPointF(n(2), n(3)));
            } else if (op.t == "D" || op.t == "DD" || op.t == "DP") {
                p->save();
                QPen dp = pen;
                dp.setWidthF(1.4);
                if (op.t == "DD") dp.setDashPattern({8, 3, 2, 3});
                else dp.setDashPattern({4, 3});
                p->setPen(dp);
                QPolygonF poly;
                for (int i = 0; i + 1 < op.v.size(); i += 2)
                    poly << QPointF(op.v[i], op.v[i + 1]);
                p->drawPolyline(poly);
                p->restore();
            } else if (op.t == "R") {
                p->drawRect(QRectF(n(0), n(1), n(2), n(3)));
            } else if (op.t == "RF") {
                p->save();
                p->setBrush(pen.color());
                p->drawRect(QRectF(n(0), n(1), n(2), n(3)));
                p->restore();
            } else if (op.t == "RR") {
                const qreal rad = qMin(n(2), n(3)) / 2;
                p->drawRoundedRect(QRectF(n(0), n(1), n(2), n(3)), rad, rad);
            } else if (op.t == "C") {
                p->drawEllipse(QPointF(n(0), n(1)), n(2), n(2));
            } else if (op.t == "F") {
                p->save();
                p->setBrush(pen.color());
                p->drawEllipse(QPointF(n(0), n(1)), n(2), n(2));
                p->restore();
            } else if (op.t == "E") {
                p->save();
                p->setBrush(QColor(op.s.isEmpty() ? QString("#ffffff") : op.s));
                p->drawEllipse(QPointF(n(0), n(1)), n(2), n(2));
                p->restore();
            } else if (op.t == "A") {
                p->drawArc(QRectF(n(0), n(1), n(2), n(3)), int(n(4) * 16), int(n(5) * 16));
            } else if (op.t == "P" || op.t == "Q" || op.t == "K") {
                QPolygonF poly;
                for (int i = 0; i + 1 < op.v.size(); i += 2)
                    poly << QPointF(op.v[i], op.v[i + 1]);
                p->save();
                if (op.t == "P") {
                    p->drawPolyline(poly);
                } else {
                    if (op.t == "K") p->setBrush(pen.color());
                    p->drawPolygon(poly);
                }
                p->restore();
            } else if (isText(op.t)) {
                text(p, op);
            }
        }
        p->setPen(base);
    }

private:
    struct Op {
        QString t;
        QVector<qreal> v;
        QString s;
    };

    static bool isText(const QString &t) { return t == "T" || t == "B" || t == "I" || t == "TL" || t == "TR"; }

    QString prop(const QString &name) const
    {
        for (const Prop &pr : props)
            if (pr.name == name)
                return pr.value;
        return QString();
    }

    // « $b0 » = première partie de « Bornes » (13-14), « $Nom » = valeur d'une propriété
    QString subst(QString t) const
    {
        t.replace('_', ' ');
        if (!t.startsWith('$'))
            return t;
        const QString key = t.mid(1);
        if (key.size() == 2 && key[0] == 'b' && key[1].isDigit())
            return prop("Bornes").split('-').value(key[1].digitValue());
        return prop(key);
    }

    void text(QPainter *p, const Op &op) const
    {
        const QString t = subst(op.s);
        if (t.isEmpty())
            return;
        if ((op.t == "TL" || op.t == "TR") && prop("Numéros") == "0")
            return;                                   // numéros de broches masqués
        const int px = qMax(4, int(op.v.value(2)));
        p->save();
        p->translate(op.v.value(0), op.v.value(1));
        p->rotate(-rotation());                       // le texte reste horizontal
        QFont f = p->font();
        f.setPixelSize(px);
        f.setBold(op.t == "B");
        f.setItalic(op.t == "I");
        p->setFont(f);
        int flags = Qt::AlignVCenter | Qt::TextDontClip;
        QRectF r(-100, -px, 200, 2 * px);
        if (op.t == "TL") { flags |= Qt::AlignLeft; r = QRectF(0, -px, 200, 2 * px); }
        else if (op.t == "TR") { flags |= Qt::AlignRight; r = QRectF(-200, -px, 200, 2 * px); }
        else flags |= Qt::AlignHCenter;
        p->drawText(r, flags, t);
        p->restore();
    }

    const LibSpec &m_s;
    QString m_role;
    QVector<QPointF> m_pins;
    QVector<Op> m_ops;
    QRectF m_rect;
};

inline SymbolItem *makeLibSymbol(const QString &kind)
{
    const LibSpec *s = libFind(kind);
    return s ? new LibItem(*s) : nullptr;
}
