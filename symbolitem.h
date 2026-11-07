#pragma once
#include <QGraphicsItem>
#include <QList>
#include <QPainter>
#include <QString>

class WireItem;

// Classe de base de tous les composants
class SymbolItem : public QGraphicsItem
{
public:
    SymbolItem();
    ~SymbolItem() override;

    QString ref;      // "R1", "R2"...
    int index = 0;    // ordre de création

    virtual int pinCount() const = 0;
    virtual QPointF pinLocal(int i) const = 0;
    QPointF pinScene(int i) const { return mapToScene(pinLocal(i)); }

    void attach(WireItem *w) { if (!m_wires.contains(w)) m_wires.append(w); }
    void detach(WireItem *w) { m_wires.removeAll(w); }
    void updateWires();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QList<WireItem *> m_wires;
};

class ResistorItem : public SymbolItem
{
public:
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }

    QRectF boundingRect() const override { return QRectF(-44, -10, 88, 20); }

    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        p->setPen(QPen(isSelected() ? QColor(0, 120, 215) : Qt::black, 2));
        p->drawLine(-40, 0, -20, 0);
        p->drawLine(20, 0, 40, 0);
        p->drawRect(QRectF(-20, -6, 40, 12));

        p->setPen(Qt::NoPen);
        p->setBrush(QColor(200, 0, 0));
        for (int i = 0; i < pinCount(); ++i)
            p->drawEllipse(pinLocal(i), 3, 3);
    }
};