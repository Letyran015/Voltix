#pragma once
#include <QGraphicsItem>
#include <QPainter>

class ResistorItem : public QGraphicsItem
{
public:
    ResistorItem()
    {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    }

    QRectF boundingRect() const override { return QRectF(-42, -10, 84, 20); }

    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        p->setPen(QPen(isSelected() ? QColor(0, 120, 215) : Qt::black, 2));
        p->drawLine(-40, 0, -20, 0);          // patte gauche
        p->drawLine(20, 0, 40, 0);            // patte droite
        p->drawRect(QRectF(-20, -6, 40, 12)); // corps
    }

protected:
    // Accroche à la grille pendant le déplacement
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == ItemPositionChange && scene()) {
            const qreal g = 20;
            QPointF p = value.toPointF();
            return QPointF(qRound(p.x() / g) * g, qRound(p.y() / g) * g);
        }
        return QGraphicsItem::itemChange(change, value);
    }
};