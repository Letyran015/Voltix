#pragma once
#include <QGraphicsItem>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QString>
#include <QStringList>
#include <QTransform>

class WireItem;

// Une propriété modifiable : R=1k Ω, C=100n F...
struct Prop {
    QString name;
    QString value;
    QString unit;          // affichage seulement
    QString description;
    bool show = false;     // afficher sous le symbole
};

class SymbolItem : public QGraphicsItem
{
public:
    SymbolItem();
    ~SymbolItem() override;

    QString ref;           // "R1", "C2"...
    int index = 0;
    bool showRef = true;
    QList<Prop> props;

    virtual QString prefix() const = 0;
    virtual QString kind() const = 0;
    virtual QString title() const = 0;       // nom affiché dans la fenêtre de propriétés
    virtual int pinCount() const = 0;
    virtual QPointF pinLocal(int i) const = 0;
    QPointF pinScene(int i) const { return mapToScene(pinLocal(i)); }

    void attach(WireItem *w) { if (!m_wires.contains(w)) m_wires.append(w); }
    void detach(WireItem *w) { m_wires.removeAll(w); }
    void updateWires();

    // grande zone pour contenir le texte ; la sélection utilise shape()
    QRectF boundingRect() const override { return QRectF(-180, -180, 360, 360); }

    QPainterPath shape() const override
    {
        QPainterPath path;
        path.addRect(bodyRect());
        return path;
    }

    QStringList labelLines() const
    {
        QStringList lines;
        if (showRef && !ref.isEmpty())
            lines << ref;
        for (const Prop &pr : props)
            if (pr.show)
                lines << QString("%1=%2 %3").arg(pr.name, pr.value, pr.unit).trimmed();
        return lines;
    }

    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        const QColor color = isSelected() ? QColor(0, 120, 215) : Qt::black;

        p->setPen(QPen(color, 2));
        p->setBrush(Qt::NoBrush);
        drawBody(p);

        p->setPen(Qt::NoPen);
        p->setBrush(QColor(200, 0, 0));
        for (int i = 0; i < pinCount(); ++i)
            p->drawEllipse(pinLocal(i), 3, 3);

        // Texte : on annule la rotation pour qu'il reste horizontal
        const QStringList lines = labelLines();
        if (lines.isEmpty())
            return;

        QTransform t;
        t.rotate(rotation());
        const QRectF rb = t.mapRect(bodyRect());     // zone occupée par le corps après rotation

        p->save();
        p->rotate(-rotation());
        QFont f = p->font();
        f.setPixelSize(12);
        p->setFont(f);
        p->setPen(color);

        const qreal h = 15.0 * lines.size();
        if (rb.height() > rb.width()) {              // symbole vertical : texte à droite
            p->drawText(QRectF(rb.right() + 8, -h / 2, 120, h),
                        Qt::AlignLeft | Qt::AlignVCenter, lines.join("\n"));
        } else {                                     // symbole horizontal : texte dessous
            p->drawText(QRectF(-60, rb.bottom() + 2, 120, h),
                        Qt::AlignHCenter | Qt::AlignTop, lines.join("\n"));
        }
        p->restore();
    }

protected:
    virtual QRectF bodyRect() const = 0;
    virtual void drawBody(QPainter *p) const = 0;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QList<WireItem *> m_wires;
};

// ---------- Résistance ----------
class ResistorItem : public SymbolItem
{
public:
    ResistorItem()
    {
        props = {
                  {"R", "1k", "Ω", "Résistance en ohms", true},
                  {"Temp", "26.85", "°C", "Température de simulation", false},
                  };
    }
    QString prefix() const override { return "R"; }
    QString kind() const override { return "resistor"; }
    QString title() const override { return "Résistance"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -10, 88, 20); }
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
    CapacitorItem()
    {
        props = {
                  {"C", "100n", "F", "Capacité en farads", true},
                  {"V", "0", "V", "Tension initiale", false},
                  };
    }
    QString prefix() const override { return "C"; }
    QString kind() const override { return "capacitor"; }
    QString title() const override { return "Condensateur"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -18, 88, 36); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -5, 0);
        p->drawLine(5, 0, 40, 0);
        p->drawLine(-5, -14, -5, 14);
        p->drawLine(5, -14, 5, 14);
    }
};

// ---------- Diode (0 = anode, 1 = cathode) ----------
class DiodeItem : public SymbolItem
{
public:
    DiodeItem()
    {
        props = {
                  {"Is", "1e-15", "A", "Courant de saturation", false},
                  {"N", "1", "", "Coefficient d'émission", false},
                  };
    }
    QString prefix() const override { return "D"; }
    QString kind() const override { return "diode"; }
    QString title() const override { return "Diode"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -16, 88, 32); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -10, 0);
        p->drawLine(10, 0, 40, 0);
        p->drawPolygon(QPolygonF({QPointF(-10, -12), QPointF(-10, 12), QPointF(10, 0)}));
        p->drawLine(10, -12, 10, 12);
    }
};

// ---------- Source de tension (0 = +, 1 = -) ----------
class VoltageSourceItem : public SymbolItem
{
public:
    VoltageSourceItem()
    {
        props = {
                  {"U", "5", "V", "Tension continue", true},
                  };
    }
    QString prefix() const override { return "V"; }
    QString kind() const override { return "vsource"; }
    QString title() const override { return "Source de tension"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -24, 88, 48); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0);
        p->drawLine(20, 0, 40, 0);
        p->drawEllipse(QPointF(0, 0), 20, 20);
        p->drawLine(-14, 0, -8, 0);
        p->drawLine(-11, -3, -11, 3);
        p->drawLine(8, 0, 14, 0);
    }
};

// ---------- Masse ----------
class GroundItem : public SymbolItem
{
public:
    GroundItem() { showRef = false; }
    QString prefix() const override { return "GND"; }
    QString kind() const override { return "ground"; }
    QString title() const override { return "Masse"; }
    int pinCount() const override { return 1; }
    QPointF pinLocal(int) const override { return QPointF(0, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-18, -6, 36, 34); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(0, 0, 0, 10);
        p->drawLine(-14, 10, 14, 10);
        p->drawLine(-9, 16, 9, 16);
        p->drawLine(-4, 22, 4, 22);
    }
};

inline SymbolItem *makeSymbol(const QString &kind)
{
    if (kind == "resistor")  return new ResistorItem;
    if (kind == "capacitor") return new CapacitorItem;
    if (kind == "diode")     return new DiodeItem;
    if (kind == "vsource")   return new VoltageSourceItem;
    if (kind == "ground")    return new GroundItem;
    return nullptr;
}