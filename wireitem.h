#pragma once
#include <QGraphicsItem>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>

class WireItem : public QGraphicsItem
{
public:
    WireItem()
    {
        setFlags(ItemIsSelectable);
        setZValue(-1);   // les fils passent sous les composants
    }

    void setPoints(const QPolygonF &p)
    {
        prepareGeometryChange();
        m_pts = p;
    }

    QRectF boundingRect() const override
    {
        return m_pts.boundingRect().adjusted(-5, -5, 5, 5);
    }

    // zone cliquable élargie pour sélectionner facilement un fil
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
    QPolygonF m_pts;
};