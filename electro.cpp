#include "symbolitem.h"

#include <QFont>
#include <QLineF>
#include <QPolygonF>
#include <QPainterPath>
#include <cmath>

// dessin commun des contacts (défini en bas) : décor 0 aucun, 1 galet, 2 niveau, 3 pression,
// 4 température, 5 bouton poussoir, 6 relais thermique, 7 arrêt d'urgence, 8 sélecteur
void voltixDrawContact(QPainter *p, const SymbolItem *it, bool nc, int decor, bool delay, const QString &bornes);

namespace {

// trait pointillé (liaison mécanique)
void dashed(QPainter *p, const QLineF &l)
{
    p->save();
    QPen pen = p->pen();
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(1.5);
    p->setPen(pen);
    p->drawLine(l);
    p->restore();
}

// texte qui reste horizontal quelle que soit la rotation du symbole
void upright(QPainter *p, const QGraphicsItem *it, const QPointF &c, const QString &t, int px = 11)
{
    p->save();
    p->translate(c);
    p->rotate(-it->rotation());
    QFont f = p->font();
    f.setPixelSize(px);
    f.setBold(false);
    p->setFont(f);
    p->drawText(QRectF(-30, -px, 60, 2 * px), Qt::AlignCenter, t);
    p->restore();
}

// ---------- Réseau triphasé ----------
class Grid3Item : public SymbolItem
{
public:
    Grid3Item() { showRef = false; }
    QString prefix() const override { return "RES"; }
    QString kind() const override { return "grid3"; }
    QString title() const override { return "Réseau triphasé"; }
    int pinCount() const override { return 3; }
    QPointF pinLocal(int i) const override { return QPointF((i - 1) * 40, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-52, -44, 104, 48); }
    void drawBody(QPainter *p) const override
    {
        static const char *names[] = {"L1", "L2", "L3"};
        for (int i = 0; i < 3; ++i) {
            const qreal x = (i - 1) * 40;
            p->drawLine(QPointF(x, 0), QPointF(x, -20));
            p->drawLine(QPointF(x - 6, -20), QPointF(x + 6, -20));
            upright(p, this, QPointF(x, -32), names[i], 12);
        }
    }
};

// ---------- Appareils à 3 pôles : sectionneur, disjoncteur, contacteur ----------
class Pole3Item : public SymbolItem
{
public:
    enum Variant { Disconnector, Breaker, Contactor };
    explicit Pole3Item(Variant v) : m_v(v)
    {
        if (v != Contactor)
            props = {{"In", "16", "A", "Courant assigné", true}};
    }
    QString prefix() const override { return m_v == Contactor ? "KM" : "Q"; }
    QString kind() const override
    {
        return m_v == Disconnector ? "disconnector3" : m_v == Breaker ? "breaker3" : "contactor3";
    }
    QString title() const override
    {
        return m_v == Disconnector ? "Sectionneur tripolaire"
               : m_v == Breaker ? "Disjoncteur tripolaire" : "Contacteur (pôles de puissance)";
    }
    int pinCount() const override { return 6; }
    QPointF pinLocal(int i) const override { return QPointF((i % 3 - 1) * 40, i < 3 ? -40 : 40); }
protected:
    QRectF bodyRect() const override { return QRectF(-52, -44, 104, 88); }
    void drawBody(QPainter *p) const override
    {
        for (int i = 0; i < 3; ++i) {
            const qreal x = (i - 1) * 40;
            p->drawLine(QPointF(x, -40), QPointF(x, -12));
            p->drawLine(QPointF(x, 40), QPointF(x, 12));
            p->drawLine(QPointF(x, 12), QPointF(x - 10, -10));      // lame ouverte
            if (m_v == Breaker) {                                   // croix du disjoncteur
                p->drawLine(QPointF(x - 14, -14), QPointF(x - 6, -6));
                p->drawLine(QPointF(x - 14, -6), QPointF(x - 6, -14));
            } else if (m_v == Disconnector) {                       // barre du sectionneur
                p->drawLine(QPointF(x - 6, -12), QPointF(x + 6, -12));
            } else {                                                // demi-cercle du contacteur
                p->drawArc(QRectF(x - 9, -3, 8, 8), 90 * 16, 180 * 16);
            }
        }
        dashed(p, QLineF(-45, 1, 35, 1));                           // liaison mécanique
    }
private:
    Variant m_v;
};

// ---------- Fusible ----------
class FuseItem : public SymbolItem
{
public:
    FuseItem() { props = {{"In", "10", "A", "Calibre", true}}; }
    QString prefix() const override { return "F"; }
    QString kind() const override { return "fuse"; }
    QString title() const override { return "Fusible"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return QPointF(0, i == 0 ? -40 : 40); }
protected:
    QRectF bodyRect() const override { return QRectF(-10, -44, 20, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawRect(QRectF(-6, -20, 12, 40));
        p->drawLine(QPointF(0, -40), QPointF(0, 40));
    }
};

// ---------- Relais thermique (puissance) ----------
class Thermal3Item : public SymbolItem
{
public:
    Thermal3Item() { props = {{"Ir", "6", "A", "Réglage du relais", true}}; }
    QString prefix() const override { return "F"; }
    QString kind() const override { return "thermal3"; }
    QString title() const override { return "Relais thermique (puissance)"; }
    int pinCount() const override { return 6; }
    QPointF pinLocal(int i) const override { return QPointF((i % 3 - 1) * 40, i < 3 ? -40 : 40); }
protected:
    QRectF bodyRect() const override { return QRectF(-52, -44, 104, 88); }
    void drawBody(QPainter *p) const override
    {
        for (int i = 0; i < 3; ++i) {
            const qreal x = (i - 1) * 40;
            p->drawLine(QPointF(x, -40), QPointF(x, -14));
            p->drawLine(QPointF(x, 14), QPointF(x, 40));
            p->drawRect(QRectF(x - 8, -14, 16, 28));
            QPainterPath z;                                          // bilame
            z.moveTo(x, -12);
            z.cubicTo(x + 9, -11, x + 9, -2, x, -2);
            z.cubicTo(x - 9, -1, x - 9, 8, x, 8);
            z.lineTo(x, 12);
            p->save();
            p->setBrush(Qt::NoBrush);
            p->drawPath(z);
            p->restore();
        }
    }
};

// ---------- Moteur triphasé ----------
class Motor3Item : public SymbolItem
{
public:
    Motor3Item()
    {
        props = {
                  {"P", "1.5", "kW", "Puissance", true},
                  {"Un", "400", "V", "Tension nominale", false},
                  };
    }
    QString prefix() const override { return "M"; }
    QString kind() const override { return "motor3"; }
    QString title() const override { return "Moteur triphasé"; }
    int pinCount() const override { return 3; }
    QPointF pinLocal(int i) const override { return QPointF((i - 1) * 40, -60); }
protected:
    QRectF bodyRect() const override { return QRectF(-52, -64, 104, 114); }
    void drawBody(QPainter *p) const override
    {
        p->drawEllipse(QPointF(0, 0), 50, 50);
        p->drawEllipse(QPointF(0, 0), 44, 44);                      // double anneau (carcasse)
        for (int i = 0; i < 3; ++i) {
            const qreal x = (i - 1) * 40;
            p->drawLine(QPointF(x, -60), QPointF(x, -std::sqrt(2500.0 - x * x)));
        }
        upright(p, this, QPointF(0, -8), "M", 20);
        upright(p, this, QPointF(0, 16), "3~", 14);
    }
};

// ---------- Bobine de contacteur / de relais ----------
class CoilItem : public SymbolItem
{
public:
    explicit CoilItem(bool relay) : m_relay(relay)
    {
        props = {{"U", "24", "V", "Tension de la bobine", true}};
    }
    QString prefix() const override { return m_relay ? "KA" : "KM"; }
    QString kind() const override { return m_relay ? "relay" : "coil"; }
    QString title() const override { return m_relay ? "Bobine de relais" : "Bobine de contacteur"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return QPointF(0, i == 0 ? -40 : 40); }
protected:
    QRectF bodyRect() const override { return QRectF(-26, -44, 62, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(QPointF(0, -40), QPointF(0, -12));
        p->drawLine(QPointF(0, 12), QPointF(0, 40));
        p->drawRect(QRectF(-24, -12, 48, 24));
        upright(p, this, QPointF(10, -30), "A1", 8);
        upright(p, this, QPointF(10, 30), "A2", 8);
    }
private:
    bool m_relay;
};

// ---------- Contacts : NO, NC, boutons poussoirs, contact thermique ----------
class ContactItem : public SymbolItem
{
public:
    enum Variant { No, Nc, PbNo, PbNc, ThNc };
    explicit ContactItem(Variant v) : m_v(v)
    {
        switch (v) {
        case No:
            props = {{"Bobine", "KM1", "", "Bobine ou relais qui commande ce contact", true},
                     {"Bornes", "13-14", "", "Repères des bornes", true}};
            showRef = false;
            break;
        case Nc:
            props = {{"Bobine", "KM1", "", "Bobine ou relais qui commande ce contact", true},
                     {"Bornes", "21-22", "", "Repères des bornes", true}};
            showRef = false;
            break;
        case ThNc:
            props = {{"Bobine", "F1", "", "Relais thermique associé", true},
                     {"Bornes", "95-96", "", "Repères des bornes", true}};
            showRef = false;
            break;
        case PbNo:
            props = {{"Bornes", "3-4", "", "Repères des bornes", true}};
            break;
        case PbNc:
            props = {{"Bornes", "1-2", "", "Repères des bornes", true}};
            break;
        }
        for (Prop &pr : props)                       // les repères sont dessinés près des bornes
            if (pr.name == "Bornes") pr.show = false;
    }
    QString prefix() const override
    {
        return (m_v == PbNo || m_v == PbNc) ? "S" : m_v == ThNc ? "FC" : "KC";
    }
    QString kind() const override
    {
        switch (m_v) {
        case No: return "contact_no";
        case Nc: return "contact_nc";
        case PbNo: return "pushbutton_no";
        case PbNc: return "pushbutton_nc";
        default: return "thermal_nc";
        }
    }
    QString title() const override
    {
        switch (m_v) {
        case No: return "Contact NO";
        case Nc: return "Contact NC";
        case PbNo: return "Bouton poussoir NO";
        case PbNc: return "Bouton poussoir NC";
        default: return "Contact de relais thermique (NC)";
        }
    }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return QPointF(0, i == 0 ? -40 : 40); }
protected:
    QRectF bodyRect() const override { return QRectF(-48, -44, 76, 88); }
    void drawBody(QPainter *p) const override
    {
        QString b;
        for (const Prop &pr : props)
            if (pr.name == "Bornes") b = pr.value;
        const bool nc = (m_v == Nc || m_v == PbNc || m_v == ThNc);
        const int decor = (m_v == PbNo || m_v == PbNc) ? 5 : m_v == ThNc ? 6 : 0;
        voltixDrawContact(p, this, nc, decor, false, b);
    }
private:
    Variant m_v;
};

// ---------- Voyant ----------
class LampItem : public SymbolItem
{
public:
    LampItem() { props = {{"Couleur", "vert", "", "Couleur du voyant", true}}; }
    QString prefix() const override { return "H"; }
    QString kind() const override { return "lamp"; }
    QString title() const override { return "Voyant"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return QPointF(0, i == 0 ? -40 : 40); }
protected:
    QRectF bodyRect() const override { return QRectF(-18, -44, 36, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(QPointF(0, -40), QPointF(0, -14));
        p->drawLine(QPointF(0, 14), QPointF(0, 40));
        p->drawEllipse(QPointF(0, 0), 14, 14);
        p->drawLine(QPointF(-10, -10), QPointF(10, 10));
        p->drawLine(QPointF(-10, 10), QPointF(10, -10));
        upright(p, this, QPointF(11, -31), "X1", 7);
        upright(p, this, QPointF(11, 31), "X2", 7);
    }
};
// ---------- Transformateur (0-1 primaire, 2-3 secondaire) ----------
class TransformerItem : public SymbolItem
{
public:
    explicit TransformerItem(bool iec = false) : m_iec(iec)
    {
        props = {
                  {"Lp", "10m", "H", "Inductance du primaire", true},
                  {"Ls", "10m", "H", "Inductance du secondaire", true},
                  {"k", "0.99", "", "Coefficient de couplage (< 1)", false},
                  };
    }
    QString prefix() const override { return "T"; }
    QString kind() const override { return m_iec ? "transformer_iec" : "transformer"; }
    QString title() const override { return m_iec ? "Transformateur (deux cercles)" : "Transformateur"; }
    int pinCount() const override { return 4; }
    QPointF pinLocal(int i) const override
    {
        if (m_iec)                                   // 0-1 primaire (haut), 2-3 secondaire (bas)
            return QPointF(i % 2 == 0 ? -20 : 20, i < 2 ? -60 : 60);
        return QPointF(i < 2 ? -20 : 20, (i % 2 == 0) ? -40 : 40);
    }
protected:
    QRectF bodyRect() const override
    {
        return m_iec ? QRectF(-34, -64, 68, 128) : QRectF(-34, -44, 68, 88);
    }
    void drawBody(QPainter *p) const override
    {
        if (m_iec) {
            p->save();
            p->setBrush(Qt::NoBrush);
            p->drawEllipse(QPointF(0, -14), 26, 26);
            p->drawEllipse(QPointF(0, 14), 26, 26);
            p->restore();
            for (int s : {-20, 20}) {
                const qreal dy = std::sqrt(26.0 * 26.0 - 20.0 * 20.0);
                p->drawLine(QPointF(s, -60), QPointF(s, -14 - dy));
                p->drawLine(QPointF(s, 60), QPointF(s, 14 + dy));
            }
            return;
        }
        p->drawLine(QPointF(-20, -40), QPointF(-20, -20));
        p->drawLine(QPointF(-20, 20), QPointF(-20, 40));
        p->drawLine(QPointF(20, -40), QPointF(20, -20));
        p->drawLine(QPointF(20, 20), QPointF(20, 40));
        for (int i = 0; i < 4; ++i) {
            p->drawArc(QRectF(-25, -20 + 10 * i, 10, 10), 90 * 16, 180 * 16);    // primaire
            p->drawArc(QRectF(15, -20 + 10 * i, 10, 10), -90 * 16, 180 * 16);    // secondaire
        }
        p->drawLine(QPointF(-3, -22), QPointF(-3, 22));                          // noyau
        p->drawLine(QPointF(3, -22), QPointF(3, 22));
    }
private:
    bool m_iec;
};

} // namespace

void voltixDrawContact(QPainter *p, const SymbolItem *it, bool nc, int decor, bool delay, const QString &bornes)
{
    auto txt = [&](QPointF c, const QString &t, int px) {
        p->save();
        p->translate(c);
        p->rotate(-it->rotation());
        QFont f = p->font();
        f.setPixelSize(px);
        f.setBold(false);
        p->setFont(f);
        p->drawText(QRectF(-20, -px, 40, 2 * px), Qt::AlignCenter, t);
        p->restore();
    };
    auto dash = [&](QPointF a, QPointF b) {
        p->save();
        QPen pen = p->pen();
        pen.setStyle(Qt::DashLine);
        pen.setWidthF(1.5);
        p->setPen(pen);
        p->drawLine(a, b);
        p->restore();
    };

    // contact normalisé : NO = lame vers la gauche ; NC = plot fixe en équerre, lame vers la droite
    p->drawLine(QPointF(0, 40), QPointF(0, 12));
    if (nc) {
        p->drawLine(QPointF(0, -40), QPointF(0, -14));
        p->drawLine(QPointF(0, -14), QPointF(16, -14));
        p->drawLine(QPointF(0, 12), QPointF(12, -17));
    } else {
        p->drawLine(QPointF(0, -40), QPointF(0, -12));
        p->drawLine(QPointF(0, 12), QPointF(-12, -10));
    }
    if (delay)
        p->drawArc(QRectF(nc ? 14 : -26, -6, 10, 10), nc ? 0 : 180 * 16, 180 * 16);

    // commande, à gauche de la lame (sauf relais thermique : à droite)
    const qreal bx = nc ? 4 : -5;
    switch (decor) {
    case 1:                                                   // galet
        dash(QPointF(bx, 0), QPointF(-14, 0));
        p->drawEllipse(QPointF(-18, 0), 4, 4);
        break;
    case 2: case 3: case 4: {                                 // niveau, pression, température
        dash(QPointF(bx, 0), QPointF(-14, 0));
        p->drawRect(QRectF(-32, -8, 18, 16));
        const QString lab = decor == 2 ? QString("N") : decor == 3 ? QString("p") : QString::fromUtf8("θ");
        txt(QPointF(-23, 0), lab, 10);
        break;
    }
    case 5:                                                   // bouton poussoir : « E »
        dash(QPointF(bx, 0), QPointF(-18, 0));
        p->drawLine(QPointF(-18, -6), QPointF(-24, -6));
        p->drawLine(QPointF(-24, -6), QPointF(-24, 6));
        p->drawLine(QPointF(-24, 6), QPointF(-18, 6));
        break;
    case 6:                                                   // élément thermique
        dash(QPointF(bx, 0), QPointF(12, 0));
        p->drawLine(QPointF(12, -5), QPointF(18, -5));
        p->drawLine(QPointF(18, -5), QPointF(18, 5));
        p->drawLine(QPointF(18, 5), QPointF(12, 5));
        break;
    case 7:                                                   // arrêt d'urgence : coup de poing
        dash(QPointF(bx, 0), QPointF(-12, 0));
        p->save();
        p->setBrush(QColor(200, 0, 0));
        p->drawEllipse(QPointF(-18, 0), 6, 6);
        p->restore();
        break;
    case 8:                                                   // commande rotative / clé
        dash(QPointF(bx, 0), QPointF(-12, 0));
        p->drawEllipse(QPointF(-16, 0), 4, 4);
        break;
    default:
        break;
    }

    // repères des bornes près des extrémités (ex. 13-14, 21-22)
    const QStringList n = bornes.split('-', Qt::SkipEmptyParts);
    if (!n.isEmpty())
        txt(QPointF(nc ? -10 : 11, -33), n[0], 8);
    if (n.size() > 1)
        txt(QPointF(11, 33), n[1], 8);
}

SymbolItem *makeElectroSymbol(const QString &k)
{
    if (k == "grid3")         return new Grid3Item;
    if (k == "disconnector3") return new Pole3Item(Pole3Item::Disconnector);
    if (k == "breaker3")      return new Pole3Item(Pole3Item::Breaker);
    if (k == "contactor3")    return new Pole3Item(Pole3Item::Contactor);
    if (k == "fuse")          return new FuseItem;
    if (k == "thermal3")      return new Thermal3Item;
    if (k == "motor3")        return new Motor3Item;
    if (k == "coil")          return new CoilItem(false);
    if (k == "relay")         return new CoilItem(true);
    if (k == "contact_no")    return new ContactItem(ContactItem::No);
    if (k == "contact_nc")    return new ContactItem(ContactItem::Nc);
    if (k == "pushbutton_no") return new ContactItem(ContactItem::PbNo);
    if (k == "pushbutton_nc") return new ContactItem(ContactItem::PbNc);
    if (k == "thermal_nc")    return new ContactItem(ContactItem::ThNc);
    if (k == "lamp")          return new LampItem;
    if (k == "transformer")   return new TransformerItem;
    if (k == "transformer_iec") return new TransformerItem(true);
    return nullptr;
}