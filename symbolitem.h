#pragma once
#include <QGraphicsItem>
#include <QList>
#include <QPainter>
#include <QPolygonF>
#include <QString>

class WireItem;

// Classe de base de tous les composants
class SymbolItem : public QGraphicsItem
{
public:
    SymbolItem();
    ~SymbolItem() override;

    QString ref;      // "R1", "C2", "GND1"...
    int index = 0;    // ordre de création

    virtual QString prefix() const = 0;
    virtual QString kind() const = 0;
    virtual int pinCount() const = 0;
    virtual QPointF pinLocal(int i) const = 0;
    QPointF pinScene(int i) const { return mapToScene(pinLocal(i)); }

    void attach(WireItem *w) { if (!m_wires.contains(w)) m_wires.append(w); }
    void detach(WireItem *w) { m_wires.removeAll(w); }
    void updateWires();

    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        p->setPen(QPen(isSelected() ? QColor(0, 120, 215) : Qt::black, 2));
        p->setBrush(Qt::NoBrush);
        drawBody(p);

        p->setPen(Qt::NoPen);
        p->setBrush(QColor(200, 0, 0));
        for (int i = 0; i < pinCount(); ++i)
            p->drawEllipse(pinLocal(i), 3, 3);
    }

protected:
    virtual void drawBody(QPainter *p) const = 0;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QList<WireItem *> m_wires;
};

// ---------- Résistance ----------
class ResistorItem : public SymbolItem
{
public:
    QString prefix() const override { return "R"; }
    QString kind() const override { return "resistor"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
    QRectF boundingRect() const override { return QRectF(-44, -10, 88, 20); }
protected:
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0);
        p->drawLine(20, 0, 40, 0);
        p->drawRect(QRectF(-20, -6, 40, 12));
    }
};

// ---------- Condensateur ----------
class CapacitorItem : public SymbolItem
{
public:
    QString prefix() const override { return "C"; }
    QString kind() const override { return "capacitor"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
    QRectF boundingRect() const override { return QRectF(-44, -18, 88, 36); }
protected:
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -5, 0);
        p->drawLine(5, 0, 40, 0);
        p->drawLine(-5, -14, -5, 14);   // armature gauche
        p->drawLine(5, -14, 5, 14);     // armature droite
    }
};

// ---------- Diode (0 = anode, 1 = cathode) ----------
class DiodeItem : public SymbolItem
{
public:
    QString prefix() const override { return "D"; }
    QString kind() const override { return "diode"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
    QRectF boundingRect() const override { return QRectF(-44, -16, 88, 32); }
protected:
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -10, 0);
        p->drawLine(10, 0, 40, 0);
        p->drawPolygon(QPolygonF({QPointF(-10, -12), QPointF(-10, 12), QPointF(10, 0)}));
        p->drawLine(10, -12, 10, 12);   // barre côté cathode
    }
};

// ---------- Source de tension (0 = +, 1 = -) ----------
class VoltageSourceItem : public SymbolItem
{
public:
    QString prefix() const override { return "V"; }
    QString kind() const override { return "vsource"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
    QRectF boundingRect() const override { return QRectF(-44, -24, 88, 48); }
protected:
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0);
        p->drawLine(20, 0, 40, 0);
        p->drawEllipse(QPointF(0, 0), 20, 20);
        p->drawLine(-14, 0, -8, 0);     // +
        p->drawLine(-11, -3, -11, 3);
        p->drawLine(8, 0, 14, 0);       // -
    }
};

// ---------- Masse (une seule broche, en haut) ----------
class GroundItem : public SymbolItem
{
public:
    QString prefix() const override { return "GND"; }
    QString kind() const override { return "ground"; }
    int pinCount() const override { return 1; }
    QPointF pinLocal(int) const override { return QPointF(0, 0); }
    QRectF boundingRect() const override { return QRectF(-18, -6, 36, 34); }
protected:
    void drawBody(QPainter *p) const override
    {
        p->drawLine(0, 0, 0, 10);
        p->drawLine(-14, 10, 14, 10);
        p->drawLine(-9, 16, 9, 16);
        p->drawLine(-4, 22, 4, 22);
    }
};

// Fabrique : crée un composant à partir de son identifiant
inline SymbolItem *makeSymbol(const QString &kind)
{
    if (kind == "resistor")  return new ResistorItem;
    if (kind == "capacitor") return new CapacitorItem;
    if (kind == "diode")     return new DiodeItem;
    if (kind == "vsource")   return new VoltageSourceItem;
    if (kind == "ground")    return new GroundItem;
    return nullptr;
}