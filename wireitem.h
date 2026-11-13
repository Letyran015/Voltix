#pragma once
#include <QColor>
#include <QGraphicsItem>
#include <QLineF>
#include <QList>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QString>
#include "symbolitem.h"

// Nature d'un fil et couleur proposée (modifiable fil par fil)
struct WireNature { const char *id; const char *label; QRgb rgb; };

inline const QList<WireNature> &wireNatures()
{
    static const QList<WireNature> n = {
                                         {"OTHER",  "Autre / signal",        qRgb(0, 110, 0)},
                                         {"L1",     "Phase L1 (marron)",     qRgb(120, 70, 30)},
                                         {"L2",     "Phase L2 (noir)",       qRgb(20, 20, 20)},
                                         {"L3",     "Phase L3 (gris)",       qRgb(130, 130, 130)},
                                         {"N",      "Neutre (bleu clair)",   qRgb(70, 150, 255)},
                                         {"PE",     "Terre PE (vert/jaune)", qRgb(0, 150, 60)},
                                         {"CMD_AC", "Commande AC (rouge)",   qRgb(210, 0, 0)},
                                         {"DC_P",   "DC + (bleu)",           qRgb(0, 60, 200)},
                                         {"DC_N",   "DC 0 V (bleu foncé)",   qRgb(0, 30, 120)},
                                         };
    return n;
}

inline QColor wireNatureColor(const QString &id)
{
    for (const WireNature &n : wireNatures())
        if (id == QLatin1String(n.id))
            return QColor(n.rgb);
    return QColor(wireNatures().first().rgb);
}

inline QString wireNatureLabel(const QString &id)
{
    for (const WireNature &n : wireNatures())
        if (id == QLatin1String(n.id))
            return QString::fromUtf8(n.label);
    return id;
}

class WireItem : public QGraphicsItem
{
public:
    WireItem()
    {
        setFlags(ItemIsSelectable);
        setZValue(-1);
        m_color = wireNatureColor(m_nature);
    }

    ~WireItem() override
    {
        if (m_a.sym) m_a.sym->detach(this);
        if (m_b.sym) m_b.sym->detach(this);
    }

    SymbolItem *symA() const { return m_a.sym; }
    int pinA() const { return m_a.pin; }
    SymbolItem *symB() const { return m_b.sym; }
    int pinB() const { return m_b.pin; }

    // propriétés électriques
    QString nature() const { return m_nature; }
    QColor color() const { return m_color; }
    QString section() const { return m_section; }
    QString mark() const { return m_mark; }
    void setNature(const QString &id, bool resetColor = true)
    {
        m_nature = id;
        if (resetColor) m_color = wireNatureColor(id);
        refresh();
    }
    void setColor(const QColor &c) { m_color = c; refresh(); }
    void setSection(const QString &s) { m_section = s; refresh(); }
    void setMark(const QString &m) { prepareGeometryChange(); m_mark = m; refresh(); }

    void setPoints(const QPolygonF &p)
    {
        prepareGeometryChange();
        m_pts = p;
    }
    const QPolygonF &points() const { return m_pts; }

    bool touches(const QPointF &p, qreal tol = 0.6) const
    {
        for (int i = 0; i + 1 < m_pts.size(); ++i) {
            const QPointF a = m_pts[i], b = m_pts[i + 1];
            const QPointF d = b - a;
            const qreal l2 = d.x() * d.x() + d.y() * d.y();
            qreal t = 0;
            if (l2 > 1e-9)
                t = qBound(0.0, ((p.x() - a.x()) * d.x() + (p.y() - a.y()) * d.y()) / l2, 1.0);
            const QPointF q = a + t * d;
            if (QLineF(p, q).length() <= tol)
                return true;
        }
        return false;
    }

    void setEnds(SymbolItem *a, int pa, SymbolItem *b, int pb)
    {
        m_a = {a, pa};
        m_b = {b, pb};
        if (a) a->attach(this);
        if (b) b->attach(this);
    }

    void detachSymbol(SymbolItem *s)
    {
        if (m_a.sym == s) m_a = {};
        if (m_b.sym == s) m_b = {};
    }

    void syncEnds()
    {
        if (m_pts.size() < 2) return;
        prepareGeometryChange();
        if (m_a.sym) moveEnd(true,  m_a.sym->pinScene(m_a.pin));
        if (m_b.sym) moveEnd(false, m_b.sym->pinScene(m_b.pin));
    }

    QRectF boundingRect() const override
    {
        return m_pts.boundingRect().adjusted(-40, -30, 70, 30);
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        path.addPolygon(m_pts);
        QPainterPathStroker s;
        s.setWidth(10);
        return s.createStroke(path);
    }

    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        const bool sel = isSelected();
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(sel ? QColor(0, 120, 215) : m_color, 2));
        p->drawPolyline(m_pts);
        if (m_nature == "PE" && !sel) {                      // vert/jaune
            QPen y(QColor(240, 200, 0), 2);
            y.setDashPattern({3.0, 3.0});
            p->setPen(y);
            p->drawPolyline(m_pts);
        }
        if (!m_mark.isEmpty() && m_pts.size() >= 2)
            drawMark(p);
    }

private:
    struct End { SymbolItem *sym = nullptr; int pin = -1; };

    void refresh()
    {
        setToolTip(QString("%1 — section %2 mm²")
                       .arg(wireNatureLabel(m_nature), m_section.isEmpty() ? "?" : m_section));
        update();
    }

    void drawMark(QPainter *p) const
    {
        int best = 0;
        qreal len = -1;
        for (int i = 0; i + 1 < m_pts.size(); ++i) {
            const qreal l = QLineF(m_pts[i], m_pts[i + 1]).length();
            if (l > len) { len = l; best = i; }
        }
        const QPointF a = m_pts[best], b = m_pts[best + 1], c = (a + b) / 2;
        QFont f = p->font();
        f.setPixelSize(10);
        f.setBold(false);
        p->setFont(f);
        p->setPen(QColor(0, 70, 200));
        if (qAbs(a.y() - b.y()) < 0.01)
            p->drawText(QRectF(c.x() - 30, c.y() - 16, 60, 14), Qt::AlignCenter, m_mark);
        else
            p->drawText(QRectF(c.x() + 4, c.y() - 7, 60, 14), Qt::AlignLeft | Qt::AlignVCenter, m_mark);
    }

    void moveEnd(bool first, const QPointF &q)
    {
        const int n = m_pts.size();
        if (n == 2) {
            const QPointF other = first ? m_pts[1] : m_pts[0];
            if (!qFuzzyCompare(q.x() + 1, other.x() + 1) && !qFuzzyCompare(q.y() + 1, other.y() + 1)) {
                const QPointF corner(q.x(), other.y());
                QPolygonF np;
                if (first) np << q << corner << other;
                else       np << other << corner << q;
                m_pts = np;
            } else {
                m_pts[first ? 0 : 1] = q;
            }
            return;
        }
        const int i = first ? 0 : n - 1;
        const int j = first ? 1 : n - 2;
        const bool horizontal = qAbs(m_pts[i].y() - m_pts[j].y()) < 0.01;
        if (horizontal) m_pts[j].setY(q.y());
        else            m_pts[j].setX(q.x());
        m_pts[i] = q;
    }

    QPolygonF m_pts;
    End m_a, m_b;
    QString m_nature = "OTHER", m_section, m_mark;
    QColor m_color;
};