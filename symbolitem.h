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
#include <QRegularExpression>
#include <QMap>
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
                  {"Rs", "0", "ohm", "Resistance serie", false},
                  {"Cjo", "0", "F", "Capacite de jonction", false},
                  {"Tt", "0", "s", "Temps de transit", false},
                  {"Bv", "0", "V", "Tension de claquage (0 = aucune)", false},
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
    QString title() const override { return "Source de tension DC"; }
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

// ---------- Source triangle (meme aspect que les autres sources : 0 = +, 1 = -) ----------
class TriangleSourceItem : public SymbolItem
{
public:
    TriangleSourceItem()
    {
        props = {
                  {"Vmin", "0", "V", "Valeur basse", false},
                  {"Vmax", "5", "V", "Valeur haute", true},
                  {"Freq", "10k", "Hz", "Frequence", true},
                  };
    }
    QString prefix() const override { return "V"; }
    QString kind() const override { return "triangle"; }
    QString title() const override { return "Source triangle"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -24, 88, 48); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0);
        p->drawLine(20, 0, 40, 0);
        p->drawEllipse(QPointF(0, 0), 20, 20);
        p->drawLine(-33, -9, -27, -9);          // signe + pres de la broche 0
        p->drawLine(-30, -12, -30, -6);
        p->drawPolyline(QPolygonF({QPointF(-12, 6), QPointF(-6, -6), QPointF(0, 6), QPointF(6, -6), QPointF(12, 6)}));
    }
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
                  {"Nf", "1", "", "Coefficient d'emission", false},
                  {"Vaf", "0", "V", "Tension d'Early (0 = infinie)", false},
                  {"Ikf", "0", "A", "Coude fort courant du gain (0 = aucun)", false},
                  {"Rb", "0", "ohm", "Resistance de base", false},
                  {"Rc", "0", "ohm", "Resistance de collecteur", false},
                  {"Re", "0", "ohm", "Resistance d'emetteur", false},
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
                  {"Umax", "15", "V", "Tension de saturation de sortie (0 = aucune)", false},
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


// ================= Composants supplementaires (dessines pour Voltix) =================

// ---------- Diodes speciales : Zener, LED, Schottky ----------
class DiodeVariantItem : public SymbolItem
{
public:
    explicit DiodeVariantItem(const QString &k) : m_k(k)
    {
        props = {
                  {"Is", k == "led" ? "3e-19" : (k == "schottky" ? "1e-5" : "1e-15"), "A", "Courant de saturation", false},
                  {"N", k == "led" ? "2" : (k == "schottky" ? "1.05" : "1"), "", "Coefficient d'emission", false},
                  {"Rs", k == "schottky" ? "0.04" : "0", "ohm", "Resistance serie", false},
                  {"Bv", k == "zener" ? "5.1" : "0", "V", k == "zener" ? "Tension Zener" : "Tension de claquage (0 = aucune)", k == "zener"},
                  };
        if (k == "photodiode") props << Prop{"Iph", "0", "A", "Photocourant (eclairement)", true};
        if (k == "tunnel") {
            props = {{"Ip", "1m", "A", "Courant de pic", true}, {"Vp", "0.1", "V", "Tension de pic", true}};
        }
    }
    QString prefix() const override { return m_k == "led" ? "LED" : "D"; }
    QString kind() const override { return m_k; }
    QString title() const override
    {
        return m_k == "zener" ? "Diode Zener" : (m_k == "led" ? "LED" : (m_k == "photodiode" ? "Photodiode"
               : (m_k == "tunnel" ? "Diode tunnel" : "Diode Schottky")));
    }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -26, 88, 52); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -10, 0);
        p->drawLine(10, 0, 40, 0);
        p->drawPolygon(QPolygonF({QPointF(-10, -12), QPointF(-10, 12), QPointF(10, 0)}));
        if (m_k == "zener") {
            p->drawLine(10, -12, 10, 12);
            p->drawLine(10, -12, 15, -16);
            p->drawLine(10, 12, 5, 16);
        } else if (m_k == "tunnel") {
            p->drawLine(10, -12, 10, 12);
            p->drawLine(10, -12, 15, -12); p->drawLine(10, 12, 15, 12);
        } else if (m_k == "photodiode") {
            p->drawLine(10, -12, 10, 12);
            for (int i = 0; i < 2; ++i) {               // deux fleches entrantes
                const qreal x = -8 + 9 * i;
                p->drawLine(QPointF(x, -22), QPointF(x + 8, -12));
                p->drawLine(QPointF(x + 8, -12), QPointF(x + 3, -13));
                p->drawLine(QPointF(x + 8, -12), QPointF(x + 7, -17));
            }
        } else if (m_k == "schottky") {
            p->drawLine(10, -12, 10, 12);
            p->drawLine(10, -12, 15, -12); p->drawLine(15, -12, 15, -8);
            p->drawLine(10, 12, 5, 12);    p->drawLine(5, 12, 5, 8);
        } else {
            p->drawLine(10, -12, 10, 12);
            for (int i = 0; i < 2; ++i) {               // deux fleches sortantes
                const qreal y = -14 - 6 * i;
                p->drawLine(QPointF(-2 + 8 * i, y + 10), QPointF(8 + 8 * i, y));
                p->drawLine(QPointF(8 + 8 * i, y), QPointF(3 + 8 * i, y + 1));
                p->drawLine(QPointF(8 + 8 * i, y), QPointF(7 + 8 * i, y + 5));
            }
        }
    }
private:
    QString m_k;
};

// ---------- MOSFET (0 = grille, 1 = drain, 2 = source) ----------
class MosItem : public SymbolItem
{
public:
    explicit MosItem(bool p) : m_p(p)
    {
        props = {
                  {"Vto", p ? "-2" : "2", "V", "Tension de seuil", true},
                  {"Kp", "0.5", "A/V2", "Transconductance", false},
                  {"Lambda", "0", "1/V", "Modulation de longueur de canal", false},
                  };
    }
    QString prefix() const override { return "M"; }
    QString kind() const override { return m_p ? "pmos" : "nmos"; }
    QString title() const override { return m_p ? "MOSFET canal P" : "MOSFET canal N"; }
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
        p->drawLine(-40, 0, -14, 0);                  // grille
        p->drawLine(-14, -16, -14, 16);               // plaque de grille
        p->drawLine(-8, -20, -8, -8);                 // canal (3 segments)
        p->drawLine(-8, -4, -8, 4);
        p->drawLine(-8, 8, -8, 20);
        p->drawLine(-8, -14, 20, -14); p->drawLine(20, -14, 20, -40);   // drain
        p->drawLine(-8, 14, 20, 14);   p->drawLine(20, 14, 20, 40);     // source
        p->drawLine(-8, 0, 20, 0);     p->drawLine(20, 0, 20, 14);      // substrat relie a la source
        // fleche du substrat : N = vers le canal, P = vers l'exterieur
        if (m_p) { p->drawLine(4, 0, 0, -3); p->drawLine(4, 0, 0, 3); }
        else     { p->drawLine(-8, 0, -4, -3); p->drawLine(-8, 0, -4, 3); }
    }
private:
    bool m_p;
};

// ---------- JFET (0 = grille, 1 = drain, 2 = source) ----------
class JfetItem : public SymbolItem
{
public:
    explicit JfetItem(bool p) : m_p(p)
    {
        props = {
                  {"Vto", p ? "2" : "-2", "V", "Tension de pincement", true},
                  {"Beta", "1m", "A/V2", "Transconductance", false},
                  {"Lambda", "0", "1/V", "Modulation de longueur de canal", false},
                  {"Rd", "0", "ohm", "Resistance de drain", false},
                  {"Rs", "0", "ohm", "Resistance de source", false},
                  };
    }
    QString prefix() const override { return "J"; }
    QString kind() const override { return m_p ? "pjfet" : "njfet"; }
    QString title() const override { return m_p ? "JFET canal P" : "JFET canal N"; }
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
        p->drawLine(-40, 0, -4, 0);
        p->drawLine(-4, -20, -4, 20);                  // canal
        p->drawLine(-4, -16, 20, -16); p->drawLine(20, -16, 20, -40);
        p->drawLine(-4, 16, 20, 16);   p->drawLine(20, 16, 20, 40);
        if (m_p) { p->drawLine(-16, 0, -22, -4); p->drawLine(-16, 0, -22, 4); p->drawLine(-22, -4, -22, 4); }
        else     { p->drawLine(-4, 0, -10, -4);  p->drawLine(-4, 0, -10, 4);  p->drawLine(-10, -4, -10, 4); }
    }
private:
    bool m_p;
};

// ---------- Thyristor (0 = anode, 1 = cathode, 2 = gachette) ----------
class ThyristorItem : public SymbolItem
{
public:
    ThyristorItem()
    {
        props = {
                  {"Bf", "50", "", "Gain des transistors internes (sensibilite de gachette)", false},
                  };
    }
    QString prefix() const override { return "TH"; }
    QString kind() const override { return "thyristor"; }
    QString title() const override { return "Thyristor"; }
    int pinCount() const override { return 3; }
    QPointF pinLocal(int i) const override
    {
        if (i == 0) return QPointF(0, -40);
        if (i == 1) return QPointF(0, 40);
        return QPointF(30, 20);
    }
protected:
    QRectF bodyRect() const override { return QRectF(-16, -44, 56, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(0, -40, 0, -12);
        p->drawLine(0, 12, 0, 40);
        p->drawPolygon(QPolygonF({QPointF(-12, -12), QPointF(12, -12), QPointF(0, 12)}));
        p->drawLine(-12, 12, 12, 12);
        p->drawLine(0, 12, 12, 20); p->drawLine(12, 20, 30, 20);     // gachette
    }
};

// ---------- Triac (0 = MT2, 1 = MT1, 2 = gachette) ----------
class TriacItem : public SymbolItem
{
public:
    TriacItem()
    {
        props = {
                  {"Bf", "50", "", "Gain des transistors internes (sensibilite de gachette)", false},
                  };
    }
    QString prefix() const override { return "TR"; }
    QString kind() const override { return "triac"; }
    QString title() const override { return "Triac"; }
    int pinCount() const override { return 3; }
    QPointF pinLocal(int i) const override
    {
        if (i == 0) return QPointF(0, -40);
        if (i == 1) return QPointF(0, 40);
        return QPointF(30, 24);
    }
protected:
    QRectF bodyRect() const override { return QRectF(-16, -44, 56, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(0, -40, 0, -14);
        p->drawLine(0, 14, 0, 40);
        p->drawLine(-12, -14, 12, -14); p->drawLine(-12, 14, 12, 14);
        p->drawPolygon(QPolygonF({QPointF(-12, -14), QPointF(12, -14), QPointF(0, 0)}));
        p->drawPolygon(QPolygonF({QPointF(-12, 14), QPointF(12, 14), QPointF(0, 0)}));
        p->drawLine(0, 14, 12, 24); p->drawLine(12, 24, 30, 24);     // gachette
    }
};

// ---------- Diac (2 broches, symetrique) ----------
class DiacItem : public SymbolItem
{
public:
    DiacItem()
    {
        props = {
                  {"Vbo", "32", "V", "Tension de retournement", true},
                  };
    }
    QString prefix() const override { return "DC"; }
    QString kind() const override { return "diac"; }
    QString title() const override { return "Diac"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -22, 88, 44); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -22, 0); p->drawLine(22, 0, 40, 0);
        p->drawPolygon(QPolygonF({QPointF(-22, -12), QPointF(-22, 12), QPointF(-4, 0)}));   // diode 1 vers la droite
        p->drawLine(-4, -12, -4, 12);
        p->drawPolygon(QPolygonF({QPointF(22, -12), QPointF(22, 12), QPointF(4, 0)}));      // diode 2 vers la gauche
        p->drawLine(4, -12, 4, 12);
    }
};

// ---------- Sources commandees (0 = entree+, 1 = entree-, 2 = sortie+, 3 = sortie-) ----------
class ControlledSourceItem : public SymbolItem
{
public:
    explicit ControlledSourceItem(const QString &k) : m_k(k)
    {
        const QString unit = k == "vcvs" ? "" : (k == "vccs" ? "S" : (k == "cccs" ? "" : "ohm"));
        const QString what = k == "vcvs" ? "Gain en tension" : (k == "vccs" ? "Transconductance" :
                              (k == "cccs" ? "Gain en courant" : "Transresistance"));
        props = {{"G", "1", unit, what, true}};
    }
    QString prefix() const override
    {
        return m_k == "vcvs" ? "E" : (m_k == "vccs" ? "G" : (m_k == "cccs" ? "F" : "H"));
    }
    QString kind() const override { return m_k; }
    QString title() const override
    {
        return m_k == "vcvs" ? "Source de tension commandee en tension"
             : m_k == "vccs" ? "Source de courant commandee en tension"
             : m_k == "cccs" ? "Source de courant commandee en courant"
                             : "Source de tension commandee en courant";
    }
    int pinCount() const override { return 4; }
    QPointF pinLocal(int i) const override
    {
        return QPointF(i < 2 ? -50 : 50, (i % 2 == 0) ? -20 : 20);
    }
protected:
    QRectF bodyRect() const override { return QRectF(-54, -30, 108, 60); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-50, -20, -30, -20); p->drawLine(-50, 20, -30, 20);
        p->drawLine(50, -20, 22, -20);   p->drawLine(50, 20, 22, 20);
        // entree : rectangle (tension) ou fil traversant (courant)
        const bool cur = (m_k == "cccs" || m_k == "ccvs");
        if (cur) p->drawLine(-30, -20, -30, 20);
        else     p->drawRect(QRectF(-30, -20, 14, 40));
        // sortie : losange
        p->drawPolygon(QPolygonF({QPointF(22, -20), QPointF(42, 0), QPointF(22, 20), QPointF(2, 0)}));
        if (m_k == "vcvs" || m_k == "ccvs") {            // + et -
            p->drawLine(19, -8, 25, -8); p->drawLine(22, -11, 22, -5);
            p->drawLine(19, 8, 25, 8);
        } else {                                          // fleche de courant
            p->drawLine(22, 9, 22, -9);
            p->drawLine(22, -9, 19, -4); p->drawLine(22, -9, 25, -4);
        }
    }
private:
    QString m_k;
};

// ---------- Source de courant continu (0 = entree du courant, 1 = sortie) ----------
class CurrentSourceItem : public SymbolItem
{
public:
    CurrentSourceItem()
    {
        props = {{"I", "10m", "A", "Courant", true}};
    }
    QString prefix() const override { return "I"; }
    QString kind() const override { return "isource"; }
    QString title() const override { return "Source de courant continu"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(0, -40) : QPointF(0, 40); }
protected:
    QRectF bodyRect() const override { return QRectF(-18, -44, 36, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(0, -40, 0, -18); p->drawLine(0, 18, 0, 40);
        p->drawEllipse(QPointF(0, 0), 18, 18);
        p->drawLine(0, 10, 0, -10);                       // fleche vers le haut
        p->drawLine(0, -10, -4, -4); p->drawLine(0, -10, 4, -4);
    }
};

// ---------- Potentiometre (0 = A, 1 = B, 2 = curseur) ----------
class PotItem : public SymbolItem
{
public:
    PotItem()
    {
        props = {
                  {"R", "10k", "ohm", "Resistance totale", true},
                  {"Pos", "0.5", "", "Position du curseur (0 = cote A, 1 = cote B)", true},
                  };
    }
    QString prefix() const override { return "RV"; }
    QString kind() const override { return "pot"; }
    QString title() const override { return "Potentiometre"; }
    int pinCount() const override { return 3; }
    QPointF pinLocal(int i) const override
    {
        if (i == 0) return QPointF(-40, 0);
        if (i == 1) return QPointF(40, 0);
        return QPointF(0, -30);
    }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -34, 88, 50); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -22, 0); p->drawLine(22, 0, 40, 0);
        p->drawRect(QRectF(-22, -7, 44, 14));
        p->drawLine(0, -30, 0, -14);                      // curseur
        p->drawLine(0, -7, -4, -14); p->drawLine(0, -7, 4, -14);
    }
};

// ---------- Thermistance CTN ----------
class NtcItem : public SymbolItem
{
public:
    NtcItem()
    {
        props = {
                  {"R25", "10k", "ohm", "Resistance a 25 C", true},
                  {"B", "3950", "K", "Coefficient B", false},
                  {"T", "25", "C", "Temperature", true},
                  };
    }
    QString prefix() const override { return "RT"; }
    QString kind() const override { return "ntc"; }
    QString title() const override { return "Thermistance CTN"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -22, 88, 44); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -22, 0); p->drawLine(22, 0, 40, 0);
        p->drawRect(QRectF(-22, -7, 44, 14));
        p->drawLine(-26, 18, -14, 18); p->drawLine(-14, 18, 20, -16);   // trait oblique + t
    }
};


// ---------- Pont de diodes (0 = AC1, 1 = AC2, 2 = +, 3 = -) ----------
class BridgeItem : public SymbolItem
{
public:
    BridgeItem()
    {
        props = {
                  {"Is", "76.9p", "A", "Courant de saturation", false},
                  {"N", "1.45", "", "Coefficient d'emission", false},
                  {"Rs", "42m", "ohm", "Resistance serie", false},
                  {"Bv", "1000", "V", "Tension de claquage inverse", false},
                  };
    }
    QString prefix() const override { return "BR"; }
    QString kind() const override { return "bridge"; }
    QString title() const override { return "Pont redresseur"; }
    int pinCount() const override { return 4; }
    QPointF pinLocal(int i) const override
    {
        if (i == 0) return QPointF(-40, 0);
        if (i == 1) return QPointF(40, 0);
        if (i == 2) return QPointF(0, -40);
        return QPointF(0, 40);
    }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -44, 88, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -26, 0); p->drawLine(26, 0, 40, 0);
        p->drawLine(0, -40, 0, -26); p->drawLine(0, 26, 0, 40);
        // quatre diodes, une par cote du losange (sens passant indique par le triangle)
        auto diode = [&](QPointF from, QPointF to) {
            p->drawLine(from, to);
            const QPointF d = to - from;
            const qreal len = std::hypot(d.x(), d.y());
            const QPointF u(d.x() / len, d.y() / len), nrm(-u.y(), u.x());
            const QPointF c = (from + to) / 2;
            p->setBrush(p->pen().color());
            p->drawPolygon(QPolygonF({c - u * 5 + nrm * 5, c - u * 5 - nrm * 5, c + u * 5}));
            p->setBrush(Qt::NoBrush);
            p->drawLine(c + u * 5 + nrm * 5, c + u * 5 - nrm * 5);
        };
        diode(QPointF(-26, 0), QPointF(0, -26));      // AC1 -> +
        diode(QPointF(26, 0), QPointF(0, -26));       // AC2 -> +
        diode(QPointF(0, 26), QPointF(-26, 0));       // - -> AC1
        diode(QPointF(0, 26), QPointF(26, 0));        // - -> AC2
    }
};

// ---------- IGBT ideal avec diode antiparallele (0 = grille, 1 = collecteur, 2 = emetteur) ----------
class IgbtItem : public SymbolItem
{
public:
    IgbtItem()
    {
        props = {
                  {"Vth", "5", "V", "Tension de grille de mise en conduction", false},
                  {"Ron", "50m", "ohm", "Resistance a l'etat passant", true},
                  };
    }
    QString prefix() const override { return "IG"; }
    QString kind() const override { return "igbt"; }
    QString title() const override { return "IGBT (interrupteur commande en tension)"; }
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
        p->drawLine(-40, 0, -14, 0);
        p->drawLine(-14, -14, -14, 14);                  // grille isolee
        p->drawLine(-8, -18, -8, 18);                    // canal
        p->drawLine(-8, -12, 20, -12); p->drawLine(20, -12, 20, -40);   // collecteur
        p->drawLine(-8, 12, 20, 12);   p->drawLine(20, 12, 20, 40);     // emetteur
        p->drawLine(20, 12, 10, 8); p->drawLine(20, 12, 12, 17);        // fleche d'emetteur
    }
};


// ================= Blocs generiques (logique, bascules, circuits integres, blocs de commande) =================
struct BlockPin { int x, y; QString label; bool bubble; };
struct BlockSpec {
    QString title, prefix, center;
    QRectF box;
    QList<BlockPin> pins;
    QList<Prop> props;
};

inline BlockSpec blockSpec(const QString &k)
{
    BlockSpec b;
    const QString Vh = "5";
    if (false) {}
    else if (false) {
    } else if (k == "timer555") {
        b.title = "Temporisateur 555"; b.prefix = "U"; b.center = "555";
        b.box = QRectF(-40, -60, 80, 120);
        // ordre des broches : 0 GND, 1 TRIG, 2 OUT, 3 RESET, 4 CTRL, 5 THR, 6 DISCH, 7 VCC
        b.pins = {{0, 80, "GND", false}, {-60, -40, "TRIG", false}, {60, -20, "OUT", false},
                  {-60, 20, "RST", false}, {-60, 0, "CTRL", false}, {-60, -20, "THR", false},
                  {60, 20, "DIS", false}, {0, -80, "VCC", false}};
    } else if (k == "tl431") {
        b.title = "Reference ajustable TL431"; b.prefix = "U"; b.center = "TL431";
        b.box = QRectF(-40, -30, 80, 60);
        b.pins = {{-60, 0, "REF", false}, {0, -40, "K", false}, {0, 40, "A", false}};
    } else if (k == "opto") {
        b.title = "Optocoupleur"; b.prefix = "U"; b.center = "opto";
        b.box = QRectF(-30, -30, 60, 60);
        b.pins = {{-50, -20, "A", false}, {-50, 20, "K", false}, {50, -20, "C", false}, {50, 20, "E", false}};
        b.props = {{"CTR", "1", "", "Rapport de transfert (1 = 100 %)", true}};
    } else if (k == "vswitch") {
        b.title = "Interrupteur commande en tension"; b.prefix = "S"; b.center = "S";
        b.box = QRectF(-30, -30, 60, 60);
        b.pins = {{-50, -20, "+", false}, {-50, 20, "-", false}, {50, -20, "", false}, {50, 20, "", false}};
        b.props = {{"Vt", "2.5", "V", "Tension de commande de fermeture", true},
                   {"Ron", "10m", "ohm", "Resistance fermee", false},
                   {"Roff", "100meg", "ohm", "Resistance ouverte", false}};
    } else if (k == "summer") {
        b.title = "Sommateur"; b.prefix = "SM"; b.center = "+";
        b.box = QRectF(-30, -30, 60, 60);
        b.pins = {{-50, -20, "", false}, {-50, 20, "", false}, {50, 0, "", false}};
        b.props = {{"G1", "1", "", "Coefficient de l'entree 1 (-1 = soustraction)", true},
                   {"G2", "1", "", "Coefficient de l'entree 2 (-1 = soustraction)", true}};
    } else if (k == "gain") {
        b.title = "Bloc de gain"; b.prefix = "K"; b.center = "K";
        b.box = QRectF(-30, -20, 60, 40);
        b.pins = {{-50, 0, "", false}, {50, 0, "", false}};
        b.props = {{"K", "1", "", "Gain", true}};
    } else if (k == "comparator") {
        b.title = "Comparateur"; b.prefix = "CP"; b.center = "CMP";
        b.box = QRectF(-30, -30, 60, 60);
        b.pins = {{-50, -20, "+", false}, {-50, 20, "-", false}, {50, 0, "", false}};
        b.props = {{"Vh", "5", "V", "Sortie niveau haut", false}, {"Vl", "0", "V", "Sortie niveau bas", false}};
    } else if (k == "quartz") {
        b.title = "Quartz"; b.prefix = "X"; b.center = "XTAL";
        b.box = QRectF(-30, -15, 60, 30);
        b.pins = {{-50, 0, "", false}, {50, 0, "", false}};
        b.props = {{"freq", "1meg", "Hz", "Frequence de resonance serie", true},
                   {"Rs", "540", "ohm", "Resistance serie", false},
                   {"Q", "1000", "", "Facteur de qualite", false},
                   {"Cp", "13p", "F", "Capacite parallele", false}};
    } else if (k == "speaker") {
        b.title = "Haut-parleur"; b.prefix = "HP"; b.center = "";
        b.box = QRectF(-30, -20, 60, 40);
        b.pins = {{-50, 0, "", false}, {50, 0, "", false}};
        b.props = {{"Re", "5", "ohm", "Resistance de la bobine", true},
                   {"fs", "35", "Hz", "Frequence de resonance", false},
                   {"Qms", "2.8", "", "Facteur de qualite mecanique", false},
                   {"Res", "32", "ohm", "Resistance de resonance", false}};
    }
    return b;
}

class BlockItem : public SymbolItem
{
public:
    explicit BlockItem(const QString &k) : m_k(k), m_s(blockSpec(k)) { props = m_s.props; }
    QString prefix() const override { return m_s.prefix; }
    QString kind() const override { return m_k; }
    QString title() const override { return m_s.title; }
    int pinCount() const override { return m_s.pins.size(); }
    QPointF pinLocal(int i) const override { return QPointF(m_s.pins[i].x, m_s.pins[i].y); }
protected:
    QRectF bodyRect() const override
    {
        QRectF r = m_s.box;
        for (const BlockPin &p : m_s.pins)
            r = r.united(QRectF(p.x - 4, p.y - 4, 8, 8));
        return r.adjusted(-4, -4, 4, 4);
    }
    void drawBody(QPainter *p) const override
    {
        const QRectF b = m_s.box;
        if (m_k == "quartz") {                       // deux plaques et un cristal entre elles
            p->drawLine(-50, 0, -9, 0); p->drawLine(9, 0, 50, 0);
            p->drawLine(-9, -13, -9, 13); p->drawLine(9, -13, 9, 13);
            p->drawRect(QRectF(-4, -9, 8, 18));
            return;
        }
        p->drawRect(b);
        if (m_k == "speaker") {                      // bobine + cone
            p->drawRect(QRectF(-14, -6, 10, 12));
            p->drawPolygon(QPolygonF({QPointF(-4, -6), QPointF(-4, 6), QPointF(10, 14), QPointF(10, -14)}));
        }
        QFont f = p->font();
        for (const BlockPin &pn : m_s.pins) {
            QPointF edge(qBound(b.left(), qreal(pn.x), b.right()), qBound(b.top(), qreal(pn.y), b.bottom()));
            const bool right = pn.x > b.right() - 0.5, left = pn.x < b.left() + 0.5;
            if (pn.bubble && right) {
                p->drawEllipse(QPointF(b.right() + 4, pn.y), 4, 4);
                p->drawLine(QPointF(b.right() + 8, pn.y), QPointF(pn.x, pn.y));
            } else {
                p->drawLine(QPointF(pn.x, pn.y), edge);
            }
            if (!pn.label.isEmpty()) {
                f.setPixelSize(10); p->setFont(f);
                if (left)       p->drawText(QRectF(b.left() + 3, pn.y - 7, 28, 14), Qt::AlignLeft | Qt::AlignVCenter, pn.label);
                else if (right) p->drawText(QRectF(b.right() - 31, pn.y - 7, 28, 14), Qt::AlignRight | Qt::AlignVCenter, pn.label);
                else if (pn.y < b.center().y()) p->drawText(QRectF(pn.x - 20, b.top() + 2, 40, 12), Qt::AlignHCenter | Qt::AlignTop, pn.label);
                else            p->drawText(QRectF(pn.x - 20, b.bottom() - 14, 40, 12), Qt::AlignHCenter | Qt::AlignBottom, pn.label);
            }
        }
        if (!m_s.center.isEmpty()) {
            f.setPixelSize(m_s.center.size() > 3 ? 11 : 15); f.setBold(true); p->setFont(f);
            p->drawText(b, Qt::AlignCenter, m_s.center);
        }
    }
private:
    QString m_k;
    BlockSpec m_s;
};


// ---------- Entree logique (interrupteur 0/1) et horloge : une seule broche de sortie ----------
class LogicSourceItem : public SymbolItem
{
public:
    explicit LogicSourceItem(bool clock) : m_clock(clock)
    {
        if (clock) props = {{"Freq", "1k", "Hz", "Frequence", true}, {"Vh", "5", "V", "Niveau haut", false}};
        else       props = {{"State", "1", "", "Etat (0 ou 1)", true}, {"Vh", "5", "V", "Niveau haut", false}};
    }
    QString prefix() const override { return m_clock ? "CK" : "IN"; }
    QString kind() const override { return m_clock ? "clock" : "lsrc"; }
    QString title() const override { return m_clock ? "Horloge" : "Entree logique"; }
    int pinCount() const override { return 1; }
    QPointF pinLocal(int) const override { return QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-34, -18, 78, 36); }
    void drawBody(QPainter *p) const override
    {
        p->drawPolygon(QPolygonF({QPointF(-30, -14), QPointF(20, -14), QPointF(36, 0), QPointF(20, 14), QPointF(-30, 14)}));
        p->drawLine(36, 0, 40, 0);
        if (m_clock) {
            p->drawPolyline(QPolygonF({QPointF(-22, 6), QPointF(-22, -6), QPointF(-12, -6), QPointF(-12, 6),
                                       QPointF(-2, 6), QPointF(-2, -6), QPointF(8, -6), QPointF(8, 6)}));
        } else {
            QString st = "1";
            for (const Prop &pr : props) if (pr.name == "State") st = pr.value.trimmed();
            p->save();
            p->rotate(-rotation());
            QFont f = p->font(); f.setPixelSize(16); f.setBold(true); p->setFont(f);
            p->drawText(QRectF(-30, -14, 50, 28), Qt::AlignCenter, st);
            p->restore();
        }
    }
private:
    bool m_clock;
};

// ---------- Sonde logique (une broche) : mesure la tension du noeud ----------
class LogicProbeItem : public MeterItem
{
public:
    LogicProbeItem() : MeterItem(false) { }
    QString prefix() const override { return "LP"; }
    QString kind() const override { return "lprobe"; }
    QString title() const override { return "Sonde logique"; }
    int pinCount() const override { return 1; }
    QPointF pinLocal(int) const override { return QPointF(-40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -20, 82, 40); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0);
        p->drawPolygon(QPolygonF({QPointF(-20, -14), QPointF(18, -14), QPointF(34, 0), QPointF(18, 14), QPointF(-20, 14)}));
        p->drawEllipse(QPointF(0, 0), 7, 7);
        p->drawLine(-4, -4, 4, 4); p->drawLine(-4, 4, 4, -4);   // temoin
    }
};


// ================= Composants supplementaires de la feuille « Circuit symbols » =================
inline void drawVarArrow(QPainter *p, QPointF from, QPointF to)     // fleche oblique des composants ajustables
{
    p->drawLine(from, to);
    const QPointF d = to - from;
    const qreal len = std::hypot(d.x(), d.y());
    const QPointF u(d.x() / len, d.y() / len), nrm(-u.y(), u.x());
    p->drawLine(to, to - u * 6 + nrm * 3);
    p->drawLine(to, to - u * 6 - nrm * 3);
}

// ---------- Resistance variable (rheostat) ----------
class RheostatItem : public SymbolItem
{
public:
    RheostatItem() { props = {{"R", "1k", "ohm", "Resistance reglee", true}}; }
    QString prefix() const override { return "RH"; }
    QString kind() const override { return "rheostat"; }
    QString title() const override { return "Resistance variable (rheostat)"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -16, 88, 32); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0); p->drawLine(20, 0, 40, 0);
        p->drawRect(QRectF(-20, -6, 40, 12));
        drawVarArrow(p, QPointF(-18, 12), QPointF(18, -12));
    }
};

// ---------- Photoresistance LDR ----------
class LdrItem : public SymbolItem
{
public:
    LdrItem()
    {
        props = {
                  {"Lux", "100", "lx", "Eclairement", true},
                  {"R10", "10k", "ohm", "Resistance a 10 lx", false},
                  {"Gamma", "0.7", "", "Pente (R diminue quand l'eclairement augmente)", false},
                  };
    }
    QString prefix() const override { return "LDR"; }
    QString kind() const override { return "ldr"; }
    QString title() const override { return "Photoresistance (LDR)"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -32, 88, 64); }
    void drawBody(QPainter *p) const override
    {
        p->drawEllipse(QPointF(0, 0), 20, 20);
        p->drawLine(-40, 0, -20, 0); p->drawLine(20, 0, 40, 0);
        p->drawRect(QRectF(-14, -5, 28, 10));
        drawVarArrow(p, QPointF(-34, -28), QPointF(-22, -16));
        drawVarArrow(p, QPointF(-26, -32), QPointF(-14, -20));
    }
};

// ---------- Condensateur polarise (0 = +) ----------
class PolarCapItem : public SymbolItem
{
public:
    PolarCapItem()
    {
        props = {
                  {"C", "100u", "F", "Capacite", true},
                  {"V", "0", "V", "Tension initiale", false},
                  {"ESR", "0", "ohm", "Resistance serie equivalente", false},
                  };
    }
    QString prefix() const override { return "CE"; }
    QString kind() const override { return "cap_pol"; }
    QString title() const override { return "Condensateur polarise (electrolytique)"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -20, 88, 40); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -6, 0);
        p->drawLine(-6, -14, -6, 14);                    // armature +
        p->drawRect(QRectF(2, -14, 6, 28));              // armature -
        p->drawLine(8, 0, 40, 0);
        p->drawLine(-18, -10, -12, -10); p->drawLine(-15, -13, -15, -7);   // signe +
    }
};

// ---------- Condensateur variable ----------
class VarCapItem : public SymbolItem
{
public:
    VarCapItem() { props = {{"C", "100n", "F", "Capacite reglee", true}, {"V", "0", "V", "Tension initiale", false}}; }
    QString prefix() const override { return "CV"; }
    QString kind() const override { return "cap_var"; }
    QString title() const override { return "Condensateur variable"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -22, 88, 44); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -5, 0); p->drawLine(5, 0, 40, 0);
        p->drawLine(-5, -14, -5, 14); p->drawLine(5, -14, 5, 14);
        drawVarArrow(p, QPointF(-14, 16), QPointF(14, -16));
    }
};

// ---------- Inductances : noyau de fer, variable ----------
class InductorVariantItem : public SymbolItem
{
public:
    explicit InductorVariantItem(const QString &k) : m_k(k)
    {
        props = {{"L", "1m", "H", "Inductance", true}, {"Ic", "0", "A", "Courant initial", false}};
    }
    QString prefix() const override { return m_k == "ind_var" ? "LV" : "L"; }
    QString kind() const override { return m_k; }
    QString title() const override { return m_k == "ind_var" ? "Inductance variable" : "Inductance a noyau de fer"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -18, 88, 36); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(-40, 0, -20, 0); p->drawLine(20, 0, 40, 0);
        for (int i = 0; i < 4; ++i)
            p->drawArc(QRectF(-20 + 10 * i, -5, 10, 10), 0, 180 * 16);
        if (m_k == "ind_fe") {
            p->drawLine(-20, 4, 20, 4); p->drawLine(-20, 7, 20, 7);
        } else {
            drawVarArrow(p, QPointF(-14, 14), QPointF(14, -14));
        }
    }
private:
    QString m_k;
};

// ---------- Transformateur avec reperes de phase (points) ----------
class TransformerPhaseItem : public SymbolItem
{
public:
    explicit TransformerPhaseItem(bool opposite) : m_opp(opposite)
    {
        props = {
                  {"Lp", "10m", "H", "Inductance du primaire", true},
                  {"Ls", "10m", "H", "Inductance du secondaire", true},
                  {"k", "0.99", "", "Coefficient de couplage (< 1)", false},
                  };
    }
    QString prefix() const override { return "T"; }
    QString kind() const override { return m_opp ? "transformer_op" : "transformer_ip"; }
    QString title() const override
    {
        return m_opp ? "Transformateur (secondaire en opposition de phase)" : "Transformateur (secondaire en phase)";
    }
    int pinCount() const override { return 4; }
    QPointF pinLocal(int i) const override { return QPointF(i < 2 ? -20 : 20, (i % 2 == 0) ? -40 : 40); }
protected:
    QRectF bodyRect() const override { return QRectF(-34, -44, 68, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(QPointF(-20, -40), QPointF(-20, -20)); p->drawLine(QPointF(-20, 20), QPointF(-20, 40));
        p->drawLine(QPointF(20, -40), QPointF(20, -20));   p->drawLine(QPointF(20, 20), QPointF(20, 40));
        for (int i = 0; i < 4; ++i) {
            p->drawArc(QRectF(-25, -20 + 10 * i, 10, 10), 90 * 16, 180 * 16);
            p->drawArc(QRectF(15, -20 + 10 * i, 10, 10), -90 * 16, 180 * 16);
        }
        QPen dash = p->pen(); dash.setStyle(Qt::DashLine); dash.setWidthF(1.5);
        p->save(); p->setPen(dash);
        p->drawLine(QPointF(-3, -22), QPointF(-3, 22)); p->drawLine(QPointF(3, -22), QPointF(3, 22));   // noyau
        p->restore();
        p->setBrush(p->pen().color());                                                   // points de phase
        p->drawEllipse(QPointF(-10, -32), 3, 3);
        p->drawEllipse(QPointF(10, m_opp ? 32 : -32), 3, 3);
        p->setBrush(Qt::NoBrush);
    }
private:
    bool m_opp;
};

// ---------- Sonnerie et lampe (charges resistives) ----------
class LoadItem : public SymbolItem
{
public:
    explicit LoadItem(bool bell) : m_bell(bell)
    {
        props = {{"R", bell ? "8" : "50", "ohm", bell ? "Impedance" : "Resistance du filament", true}};
    }
    QString prefix() const override { return m_bell ? "BZ" : "LA"; }
    QString kind() const override { return m_bell ? "bell" : "bulb"; }
    QString title() const override { return m_bell ? "Sonnerie" : "Lampe"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return i == 0 ? QPointF(-40, 0) : QPointF(40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -20, 88, 40); }
    void drawBody(QPainter *p) const override
    {
        if (m_bell) {
            p->drawLine(-40, 0, -16, 0); p->drawLine(16, 0, 40, 0);
            p->drawArc(QRectF(-16, -16, 32, 32), 0, 180 * 16);
            p->drawLine(-16, 0, 16, 0);
        } else {
            p->drawLine(-40, 0, -16, 0); p->drawLine(16, 0, 40, 0);
            p->drawEllipse(QPointF(0, 0), 16, 16);
            p->drawLine(-11, -11, 11, 11); p->drawLine(-11, 11, 11, -11);
        }
    }
private:
    bool m_bell;
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
    if (kind == "triangle")  return new TriangleSourceItem;
    if (kind == "npn")       return new BjtItem(false);
    if (kind == "pnp")       return new BjtItem(true);
    if (kind == "opamp")     return new OpAmpItem;
    if (kind == "zener" || kind == "led" || kind == "schottky") return new DiodeVariantItem(kind);
    if (kind == "nmos")      return new MosItem(false);
    if (kind == "pmos")      return new MosItem(true);
    if (kind == "njfet")     return new JfetItem(false);
    if (kind == "pjfet")     return new JfetItem(true);
    if (kind == "thyristor") return new ThyristorItem;
    if (kind == "triac")     return new TriacItem;
    if (kind == "diac")      return new DiacItem;
    if (kind == "vcvs" || kind == "vccs" || kind == "cccs" || kind == "ccvs") return new ControlledSourceItem(kind);
    if (kind == "isource")   return new CurrentSourceItem;
    if (kind == "bridge")    return new BridgeItem;
    if (kind == "igbt")      return new IgbtItem;
    {
        if (kind == "lsrc")   return new LogicSourceItem(false);
        if (kind == "clock")  return new LogicSourceItem(true);
        if (kind == "lprobe") return new LogicProbeItem;
    }
    for (const char *bk : {"timer555",
                           "tl431", "opto", "vswitch", "summer", "gain", "comparator", "quartz", "speaker"})
        if (kind == bk) return new BlockItem(kind);
    if (kind == "photodiode" || kind == "tunnel") return new DiodeVariantItem(kind);
    if (kind == "rheostat")  return new RheostatItem;
    if (kind == "ldr")       return new LdrItem;
    if (kind == "cap_pol")   return new PolarCapItem;
    if (kind == "cap_var")   return new VarCapItem;
    if (kind == "ind_fe" || kind == "ind_var") return new InductorVariantItem(kind);
    if (kind == "transformer_ip") return new TransformerPhaseItem(false);
    if (kind == "transformer_op") return new TransformerPhaseItem(true);
    if (kind == "bell")      return new LoadItem(true);
    if (kind == "bulb")      return new LoadItem(false);
    if (kind == "pot")       return new PotItem;
    if (kind == "ntc")       return new NtcItem;
    if (kind == "ground")    return new GroundItem;
    if (SymbolItem *s = makeElectroSymbol(kind))
        return s;
    return makeElectroSymbol2(kind);
}