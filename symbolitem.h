#pragma once
#include <QGraphicsItem>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <cmath>
#include <QPair>

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
    virtual bool pinActive(int) const { return true; }
    virtual QList<QPair<int, int>> internalLinks() const { return {}; }
    QPointF pinScene(int i) const
    {
        return pinActive(i) ? mapToScene(pinLocal(i)) : QPointF(-4800 + 20 * (i % 200), -4800);
    }
    void propsChanged() { prepareGeometryChange(); update(); updateWires(); }
    static inline bool hidePins = false;      // vrai pendant l'export PDF

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

    virtual QStringList labelLines() const
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

        if (!hidePins) {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(200, 0, 0));
            for (int i = 0; i < pinCount(); ++i)
                if (pinActive(i))
                    p->drawEllipse(pinLocal(i), 3, 3);
        }

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

// ---------- Inductance ----------
class InductorItem : public SymbolItem
{
public:
    InductorItem()
    {
        props = {
                  {"L", "1m", "H", "Inductance en henrys", true},
                  {"Ic", "0", "A", "Courant initial", false},
                  };
    }
    QString prefix() const override { return "L"; }
    QString kind() const override { return "inductor"; }
    QString title() const override { return "Inductance"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -10, 88, 20); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0);
        p->drawLine(20, 0, 40, 0);
        for (int i = 0; i < 4; ++i)
            p->drawArc(QRectF(-20 + 10 * i, -5, 10, 10), 0, 180 * 16);   // 4 demi-cercles
    }
};

// ---------- Source sinusoïdale / impulsion (0 = +, 1 = -) ----------
class WaveSourceItem : public SymbolItem
{
public:
    explicit WaveSourceItem(bool pulse) : m_pulse(pulse)
    {
        if (pulse)
            props = {
                      {"V1", "0", "V", "Niveau bas", true},
                      {"V2", "5", "V", "Niveau haut", true},
                      {"Td", "0", "s", "Retard", false},
                      {"Tr", "1n", "s", "Temps de montée", false},
                      {"Tf", "1n", "s", "Temps de descente", false},
                      {"Pw", "500u", "s", "Durée du niveau haut", true},
                      {"Per", "1m", "s", "Période", true},
                      };
        else
            props = {
                      {"Voff", "0", "V", "Tension continue (offset)", false},
                      {"Vamp", "1", "V", "Amplitude", true},
                      {"Freq", "1k", "Hz", "Fréquence", true},
                      };
    }
    QString prefix() const override { return "V"; }
    QString kind() const override { return m_pulse ? "vpulse" : "vsin"; }
    QString title() const override { return m_pulse ? "Source impulsion" : "Source sinusoïdale"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -24, 88, 48); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0);
        p->drawLine(20, 0, 40, 0);
        p->drawEllipse(QPointF(0, 0), 20, 20);
        p->drawLine(-33, -9, -27, -9);          // signe + près de la broche 0
        p->drawLine(-30, -12, -30, -6);
        if (m_pulse) {
            QPolygonF w;
            w << QPointF(-12, 6) << QPointF(-12, -6) << QPointF(-2, -6)
              << QPointF(-2, 6) << QPointF(8, 6) << QPointF(8, -6) << QPointF(12, -6);
            p->drawPolyline(w);
        } else {
            QPolygonF w;
            for (int i = 0; i <= 24; ++i) {
                const double x = -12 + i;
                w << QPointF(x, -7 * std::sin((x + 12) / 24.0 * 2 * M_PI));
            }
            p->drawPolyline(w);
        }
    }
private:
    bool m_pulse;
};

// ---------- Transistor bipolaire (0 = base, 1 = collecteur, 2 = émetteur) ----------
class BjtItem : public SymbolItem
{
public:
    explicit BjtItem(bool pnp) : m_pnp(pnp)
    {
        props = {
                  {"Bf", "100", "", "Gain en courant", false},
                  {"Is", "1e-14", "A", "Courant de saturation", false},
                  };
    }
    QString prefix() const override { return "Q"; }
    QString kind() const override { return m_pnp ? "pnp" : "npn"; }
    QString title() const override { return m_pnp ? "Transistor PNP" : "Transistor NPN"; }
    int pinCount() const override { return 3; }
    QPointF pinLocal(int i) const override
    {
        if (i == 0) return QPointF(-40, 0);
        if (i == 1) return QPointF(20, -40);
        return QPointF(20, 40);
    }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -44, 68, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -10, 0);                            // base
        p->drawLine(-10, -20, -10, 20);                         // barre
        p->drawLine(-10, -8, 20, -20); p->drawLine(20, -20, 20, -40);   // collecteur
        p->drawLine(-10, 8, 20, 20);   p->drawLine(20, 20, 20, 40);     // émetteur

        QPolygonF a;
        if (!m_pnp) a << QPointF(17, 18.8) << QPointF(6.5, 18.9) << QPointF(9.5, 11.5);   // sort
        else        a << QPointF(-4, 10.4) << QPointF(3.5, 17.7) << QPointF(6.5, 10.3);   // entre
        p->setBrush(p->pen().color());
        p->drawPolygon(a);
        p->setBrush(Qt::NoBrush);
    }
private:
    bool m_pnp;
};

// ---------- Ampli-op (0 = entrée +, 1 = entrée -, 2 = sortie) ----------
class OpAmpItem : public SymbolItem
{
public:
    OpAmpItem()
    {
        props = {
                  {"A", "1e6", "", "Gain en boucle ouverte", false},
                  };
    }
    QString prefix() const override { return "U"; }
    QString kind() const override { return "opamp"; }
    QString title() const override { return "Amplificateur opérationnel"; }
    int pinCount() const override { return 3; }
    QPointF pinLocal(int i) const override
    {
        if (i == 0) return QPointF(-40, -20);
        if (i == 1) return QPointF(-40, 20);
        return QPointF(40, 0);
    }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -44, 88, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, -20, -20, -20);
        p->drawLine(-40, 20, -20, 20);
        p->drawPolygon(QPolygonF({QPointF(-20, -40), QPointF(-20, 40), QPointF(40, 0)}));
        p->drawLine(-17, -20, -11, -20);        // +
        p->drawLine(-14, -23, -14, -17);
        p->drawLine(-17, 20, -11, 20);          // -
    }
};

// ---------- Étiquette de nœud (une broche) ----------
class LabelItem : public SymbolItem
{
public:
    LabelItem()
    {
        showRef = false;
        props = {
                  {"Nom", "Vout", "", "Nom du nœud (lettres, chiffres, _)", false},
                  };
    }
    QString prefix() const override { return "LBL"; }
    QString kind() const override { return "label"; }
    QString title() const override { return "Étiquette de nœud"; }
    int pinCount() const override { return 1; }
    QPointF pinLocal(int) const override { return QPointF(0, 0); }
    QStringList labelLines() const override
    {
        return QStringList{props.isEmpty() ? QString() : props[0].value};
    }
protected:
    QRectF bodyRect() const override { return QRectF(-4, -9, 42, 18); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(0, 0, 12, 0);
        p->drawPolygon(QPolygonF({QPointF(12, 0), QPointF(18, -7), QPointF(34, -7),
                                  QPointF(34, 7), QPointF(18, 7)}));
    }
};

// ---------- Instruments de mesure : voltmètre (V) et ampèremètre (A) ----------
// Voltmètre : mesure U(broche +) - U(broche -), sans perturber le circuit.
// Ampèremètre : à placer EN SÉRIE ; mesure le courant qui entre par la broche + et sort par la broche -.
class MeterItem : public SymbolItem
{
public:
    explicit MeterItem(bool current) : m_current(current)
    {
        props = {
                  {"Nom", "", "", "Nom de la courbe tracée (facultatif)", false},
                  };
        showRef = true;
    }
    QString reading;                                   // dernière valeur calculée (point de fonctionnement)
    QString prefix() const override { return m_current ? "AM" : "VM"; }
    QString kind() const override { return m_current ? "ammeter" : "vmeter"; }
    QString title() const override { return m_current ? "Ampèremètre" : "Voltmètre"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
    QStringList labelLines() const override
    {
        QStringList l;
        const QString nom = props.isEmpty() ? QString() : props[0].value.trimmed();
        if (!nom.isEmpty()) l << nom;
        else if (showRef && !ref.isEmpty()) l << ref;
        if (!reading.isEmpty()) l << reading;
        return l;
    }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -24, 88, 48); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0);
        p->drawLine(20, 0, 40, 0);
        p->drawEllipse(QPointF(0, 0), 20, 20);
        p->save();
        p->rotate(-rotation());                          // lettre toujours droite
        QFont f = p->font();
        f.setPixelSize(20);
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(-14, -14, 28, 28), Qt::AlignCenter, m_current ? "A" : "V");
        p->restore();
        p->drawLine(-36, -9, -30, -9);                   // repère de la broche +
        p->drawLine(-33, -12, -33, -6);
        if (m_current) {                                 // sens du courant : broche + vers broche -
            p->drawLine(-10, 26, 10, 26);
            p->drawLine(10, 26, 5, 22);
            p->drawLine(10, 26, 5, 30);
        }
    }
private:
    bool m_current;
};

SymbolItem *makeElectroSymbol(const QString &kind);   // défini dans electro.cpp
SymbolItem *makeElectroSymbol(const QString &kind);
SymbolItem *makeElectroSymbol2(const QString &kind);

inline SymbolItem *makeSymbol(const QString &kind)
{
    if (kind == "label")     return new LabelItem;
    if (kind == "vmeter")    return new MeterItem(false);
    if (kind == "ammeter")   return new MeterItem(true);
    if (kind == "resistor")  return new ResistorItem;
    if (kind == "capacitor") return new CapacitorItem;
    if (kind == "diode")     return new DiodeItem;
    if (kind == "vsource")   return new VoltageSourceItem;
    if (kind == "ground")    return new GroundItem;
    if (kind == "resistor")  return new ResistorItem;
    if (kind == "capacitor") return new CapacitorItem;
    if (kind == "inductor")  return new InductorItem;
    if (kind == "diode")     return new DiodeItem;
    if (kind == "vsource")   return new VoltageSourceItem;
    if (kind == "vsin")      return new WaveSourceItem(false);
    if (kind == "vpulse")    return new WaveSourceItem(true);
    if (kind == "npn")       return new BjtItem(false);
    if (kind == "pnp")       return new BjtItem(true);
    if (kind == "opamp")     return new OpAmpItem;
    if (kind == "ground")    return new GroundItem;
    if (SymbolItem *s = makeElectroSymbol(kind))
        return s;
    return makeElectroSymbol2(kind);
}