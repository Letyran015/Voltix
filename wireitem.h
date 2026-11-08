#pragma once
#include <QGraphicsItem>
#include <QLineF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include "symbolitem.h"

class WireItem : public QGraphicsItem
{
public:
    WireItem()
    {
        setFlags(ItemIsSelectable);
        setZValue(-1);
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

    void setPoints(const QPolygonF &p)
    {
        prepareGeometryChange();
        m_pts = p;
    }
    const QPolygonF &points() const { return m_pts; }

    // le point p est-il sur ce fil ?
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

    // accroche les extrémités à des broches (nullptr = extrémité libre)
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

    // replace les extrémités sur leurs broches, en gardant des segments orthogonaux
    void syncEnds()
    {
        if (m_pts.size() < 2) return;
        prepareGeometryChange();
        if (m_a.sym) moveEnd(true,  m_a.sym->pinScene(m_a.pin));
        if (m_b.sym) moveEnd(false, m_b.sym->pinScene(m_b.pin));
    }

    QRectF boundingRect() const override
    {
        return m_pts.boundingRect().adjusted(-5, -5, 5, 5);
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
        p->setPen(QPen(isSelected() ? QColor(0, 120, 215) : QColor(0, 110, 0), 2));
        p->drawPolyline(m_pts);
    }

private:
    struct End { SymbolItem *sym = nullptr; int pin = -1; };

    void moveEnd(bool first, const QPointF &q)
    {
        const int n = m_pts.size();
        if (n == 2) {
            const QPointF other = first ? m_pts[1] : m_pts[0];
            if (!qFuzzyCompare(q.x() + 1, other.x() + 1) && !qFuzzyCompare(q.y() + 1, other.y() + 1)) {
                const QPointF corner(q.x(), other.y());     // crée un coude
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
};