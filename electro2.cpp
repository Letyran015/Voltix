#include "symbolitem.h"
#include "catalog.h"

#include <QFont>
#include <QLineF>
#include <QPolygonF>
#include <QRegularExpression>
#include <cmath>
#include <functional>
#include <utility>

namespace {

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

using DrawFn = std::function<void(QPainter *, const SymbolItem *)>;

// ---------- Composant à deux broches verticales, dessin fourni par une fonction ----------
class V2Item : public SymbolItem
{
public:
    V2Item(QString kind, QString title, QString prefix, DrawFn d,
           QRectF body = QRectF(-24, -44, 56, 88))
        : m_kind(std::move(kind)), m_title(std::move(title)), m_prefix(std::move(prefix)),
        m_draw(std::move(d)), m_body(body) {}
    QString prefix() const override { return m_prefix; }
    QString kind() const override { return m_kind; }
    QString title() const override { return m_title; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return QPointF(0, i == 0 ? -40 : 40); }
protected:
    QRectF bodyRect() const override { return m_body; }
    void drawBody(QPainter *p) const override { m_draw(p, this); }
private:
    QString m_kind, m_title, m_prefix;
    DrawFn m_draw;
    QRectF m_body;
};

// contacts : decor 0 aucun, 1 galet (fin de course), 2 niveau, 3 pression, 4 température
void drawContact(QPainter *p, const SymbolItem *it, bool nc, int decor, bool delay)
{
    p->drawLine(QPointF(0, 40), QPointF(0, 12));
    p->drawLine(QPointF(0, -40), QPointF(0, -12));
    if (nc) {
        p->drawLine(QPointF(0, 12), QPointF(-12, -12));
        p->drawLine(QPointF(0, -12), QPointF(-12, -12));
    } else {
        p->drawLine(QPointF(0, 12), QPointF(-12, -8));
    }
    if (delay)
        p->drawArc(QRectF(-24, -6, 10, 10), 180 * 16, 180 * 16);
    if (decor == 1) {
        dashed(p, QLineF(-6, 0, 14, 0));
        p->drawEllipse(QPointF(18, 0), 4, 4);
    } else if (decor >= 2) {
        dashed(p, QLineF(-6, 0, 10, 0));
        p->drawRect(QRectF(10, -8, 16, 16));
        const QString lab = decor == 2 ? QString("N") : decor == 3 ? QString("p")
                                                                   : QString::fromUtf8("\xCE\xB8");
        upright(p, it, QPointF(18, 0), lab, 10);
    }
}

// ---------- Appareils à n pôles ----------
class PoleDevice : public SymbolItem
{
public:
    PoleDevice(const PoleType &t, int n) : m_type(t.id), m_title(t.title), m_prefix(t.prefix), m_n(n)
    {
        if (t.id == "mcb")
            props = {{"In", "16", "A", "Courant assigné", true},
                     {"Courbe", "C", "", "Courbe de déclenchement (B, C, D)", true},
                     {"Pdc", "6", "kA", "Pouvoir de coupure", false}};
        else if (t.id == "mpcb")
            props = {{"Ir", "4-6.3", "A", "Plage de réglage", true},
                     {"Pdc", "10", "kA", "Pouvoir de coupure", false}};
        else if (t.id == "rcd")
            props = {{"In", "40", "A", "Courant assigné", true},
                     {"Idn", "30", "mA", "Sensibilité différentielle", true},
                     {"Type", "A", "", "Type (AC, A, F, B)", false}};
        else if (t.id == "rcbo")
            props = {{"In", "16", "A", "Courant assigné", true},
                     {"Courbe", "C", "", "Courbe de déclenchement", true},
                     {"Idn", "30", "mA", "Sensibilité différentielle", true}};
        else if (t.id == "surge")
            props = {{"Type", "T2", "", "Type de parafoudre", true},
                     {"Imax", "20", "kA", "Courant maximal", false}};
        else if (t.id == "fuseholder" || t.id == "fusedisc")
            props = {{"In", "10", "A", "Calibre", true},
                     {"Taille", "10x38", "", "Dimensions de la cartouche", false}};
        else if (t.id == "contactor")
            props = {{"Ie", "9", "A", "Courant d'emploi", true}};
        else
            props = {{"In", "25", "A", "Courant assigné", true}};
        if (t.id == "contactor")
            showRef = true;
    }
    QString prefix() const override { return m_prefix; }
    QString kind() const override { return QString("%1_%2p").arg(m_type).arg(m_n); }
    QString title() const override { return QString("%1 %2P").arg(m_title).arg(m_n); }
    int pinCount() const override { return 2 * m_n; }
    QPointF pinLocal(int i) const override
    {
        const int k = i % m_n;
        return QPointF((k - (m_n - 1) / 2.0) * 40, i < m_n ? -40 : 40);
    }
protected:
    QRectF bodyRect() const override
    {
        const qreal w = (m_n - 1) * 40;
        return QRectF(-w / 2 - 14, -44, w + 28, 88);
    }
    void drawBody(QPainter *p) const override
    {
        const bool brk = (m_type == "mcb" || m_type == "mpcb" || m_type == "rcbo");
        const bool fuse = (m_type == "fusedisc");
        const bool diff = (m_type == "rcd" || m_type == "rcbo");
        const qreal half = (m_n - 1) * 20;

        for (int k = 0; k < m_n; ++k) {
            const qreal x = (k - (m_n - 1) / 2.0) * 40;
            if (m_type == "fuseholder") {
                p->drawLine(QPointF(x, -40), QPointF(x, -16));
                p->drawLine(QPointF(x, 16), QPointF(x, 40));
                p->drawRect(QRectF(x - 6, -16, 12, 32));
                continue;
            }
            if (m_type == "surge") {
                p->drawLine(QPointF(x, -40), QPointF(x, -14));
                p->drawLine(QPointF(x, 14), QPointF(x, 40));
                p->drawRect(QRectF(x - 6, -14, 12, 28));
                p->drawLine(QPointF(x - 4, 9), QPointF(x + 4, -9));
                p->save();
                p->setBrush(p->pen().color());
                p->drawPolygon(QPolygonF({QPointF(x + 4, -9), QPointF(x + 3, -2), QPointF(x - 1, -6)}));
                p->restore();
                continue;
            }

            p->drawLine(QPointF(x, -40), QPointF(x, -12));
            if (brk || fuse) {                                   // élément thermique / fusible
                p->drawLine(QPointF(x, 40), QPointF(x, 32));
                p->drawRect(QRectF(x - 4, 18, 8, 14));
                p->drawLine(QPointF(x, 18), QPointF(x, 12));
            } else {
                p->drawLine(QPointF(x, 40), QPointF(x, 12));
            }
            p->drawLine(QPointF(x, 12), QPointF(x - 10, -10));   // lame ouverte

            if (brk) {                                           // croix du disjoncteur
                p->drawLine(QPointF(x - 14, -14), QPointF(x - 6, -6));
                p->drawLine(QPointF(x - 14, -6), QPointF(x - 6, -14));
            }
            if (m_type == "contactor")
                p->drawArc(QRectF(x - 9, -3, 8, 8), 90 * 16, 180 * 16);
            if (m_type == "disc" || m_type == "swdisc" || fuse)
                p->drawLine(QPointF(x - 6, -14), QPointF(x + 6, -14));          // barre du sectionneur
            if (m_type == "sw" || m_type == "swdisc" || m_type == "rcd" || fuse)
                p->drawLine(QPointF(x - 13.6, -8.4), QPointF(x - 6.4, -11.6));  // trait de l'interrupteur
        }
        if (diff)
            p->drawRect(QRectF(-half - 9, -34, 2 * half + 18, 12));             // tore différentiel
        if (m_n > 1 && m_type != "fuseholder" && m_type != "surge")
            dashed(p, QLineF(-half - 5, 1, half - 5, 1));                       // liaison mécanique
    }
private:
    QString m_type, m_title, m_prefix;
    int m_n;
};

// ---------- Terre (PE) ----------
class EarthItem : public SymbolItem
{
public:
    EarthItem() { showRef = false; }
    QString prefix() const override { return "PE"; }
    QString kind() const override { return "earth"; }
    QString title() const override { return "Terre (PE)"; }
    int pinCount() const override { return 1; }
    QPointF pinLocal(int) const override { return QPointF(0, 0); }
protected:
    QRectF bodyRect() const override { return QRectF(-18, -6, 56, 34); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(0, 0, 0, 10);
        p->drawLine(-14, 10, 14, 10);
        p->drawLine(-9, 16, 9, 16);
        p->drawLine(-4, 22, 4, 22);
        upright(p, this, QPointF(26, 14), "PE", 10);
    }
};

// ---------- Alimentation AC/DC ----------
class PsuItem : public SymbolItem
{
public:
    PsuItem()
    {
        props = {{"Uin", "400", "V AC", "Tension d'entrée", true},
                 {"Uout", "24", "V DC", "Tension de sortie", true}};
    }
    QString prefix() const override { return "G"; }
    QString kind() const override { return "psu"; }
    QString title() const override { return "Alimentation AC/DC"; }
    int pinCount() const override { return 4; }     // 0 L, 1 N, 2 +, 3 -
    QPointF pinLocal(int i) const override
    {
        return QPointF(i % 2 == 0 ? -20 : 20, i < 2 ? -40 : 40);
    }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -44, 88, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(QPointF(-20, -40), QPointF(-20, -20));
        p->drawLine(QPointF(20, -40), QPointF(20, -20));
        p->drawLine(QPointF(-20, 20), QPointF(-20, 40));
        p->drawLine(QPointF(20, 20), QPointF(20, 40));
        p->drawRect(QRectF(-40, -20, 80, 40));
        p->drawLine(QPointF(-40, 20), QPointF(40, -20));
        upright(p, this, QPointF(-24, -9), "AC", 10);
        upright(p, this, QPointF(24, 9), "DC", 10);
    }
};

// ---------- Contact inverseur (0 commun, 1 repos, 2 travail) ----------
class ChangeoverItem : public SymbolItem
{
public:
    ChangeoverItem()
    {
        showRef = false;
        props = {{"Bobine", "KM1", "", "Bobine ou relais qui commande ce contact", true},
                 {"Bornes", "11-12-14", "", "Repères des bornes", true}};
    }
    QString prefix() const override { return "KC"; }
    QString kind() const override { return "contact_co"; }
    QString title() const override { return "Contact inverseur"; }
    int pinCount() const override { return 3; }
    QPointF pinLocal(int i) const override
    {
        if (i == 0) return QPointF(0, 40);
        return QPointF(i == 1 ? -20 : 20, -40);
    }
protected:
    QRectF bodyRect() const override { return QRectF(-30, -44, 60, 88); }
    void drawBody(QPainter *p) const override
    {
        p->drawLine(QPointF(0, 40), QPointF(0, 12));
        p->drawLine(QPointF(-20, -40), QPointF(-20, -12));
        p->drawLine(QPointF(20, -40), QPointF(20, -12));
        p->drawLine(QPointF(-26, -12), QPointF(-14, -12));
        p->drawLine(QPointF(14, -12), QPointF(26, -12));
        p->drawLine(QPointF(0, 12), QPointF(-12, -8));
        dashed(p, QLineF(-6, 2, 8, 2));
    }
};

// ---------- Moteur monophasé / courant continu ----------
class MotorRound : public SymbolItem
{
public:
    explicit MotorRound(bool dc) : m_dc(dc)
    {
        props = {{"P", "0.75", "kW", "Puissance", true},
                 {"Un", dc ? "24" : "230", "V", "Tension nominale", false}};
    }
    QString prefix() const override { return "M"; }
    QString kind() const override { return m_dc ? "motordc" : "motor1"; }
    QString title() const override { return m_dc ? "Moteur à courant continu" : "Moteur monophasé"; }
    int pinCount() const override { return 2; }
    QPointF pinLocal(int i) const override { return QPointF(i == 0 ? -20 : 20, -60); }
protected:
    QRectF bodyRect() const override { return QRectF(-44, -64, 88, 108); }
    void drawBody(QPainter *p) const override
    {
        p->drawEllipse(QPointF(0, 0), 40, 40);
        const qreal y = -std::sqrt(1600.0 - 400.0);
        p->drawLine(QPointF(-20, -60), QPointF(-20, y));
        p->drawLine(QPointF(20, -60), QPointF(20, y));
        upright(p, this, QPointF(0, -6), "M", 18);
        upright(p, this, QPointF(0, 14), m_dc ? "=" : "1~", 13);
    }
private:
    bool m_dc;
};

// ---------- Capteur 3 fils (0 BN +, 1 BK sortie, 2 BU 0 V) ----------
class Sensor3Item : public SymbolItem
{
public:
    Sensor3Item(QString kind, QString title, QString letter)
        : m_kind(std::move(kind)), m_title(std::move(title)), m_letter(std::move(letter))
    {
        props = {{"Sortie", "PNP", "", "Type de sortie (PNP, NPN)", true},
                 {"Fonction", "NO", "", "NO ou NC", true}};
    }
    QString prefix() const override { return "B"; }
    QString kind() const override { return m_kind; }
    QString title() const override { return m_title; }
    int pinCount() const override { return 3; }
    QPointF pinLocal(int i) const override { return QPointF((i - 1) * 40, -40); }
protected:
    QRectF bodyRect() const override { return QRectF(-52, -44, 104, 70); }
    void drawBody(QPainter *p) const override
    {
        static const char *wires[] = {"BN", "BK", "BU"};
        for (int k = 0; k < 3; ++k) {
            const qreal x = (k - 1) * 40;
            p->drawLine(QPointF(x, -40), QPointF(x, -16));
            upright(p, this, QPointF(x + 14, -28), wires[k], 8);
        }
        p->drawRect(QRectF(-52, -16, 104, 40));
        p->drawPolygon(QPolygonF({QPointF(0, -8), QPointF(14, 4), QPointF(0, 16), QPointF(-14, 4)}));
        upright(p, this, QPointF(0, 4), m_letter, 9);
    }
private:
    QString m_kind, m_title, m_letter;
};

// ---------- Bornier : jusqu'à 24 bornes, chacune conduit de haut en bas ----------
class TerminalItem : public SymbolItem
{
public:
    static constexpr int CAP = 24;
    TerminalItem()
    {
        showRef = false;
        props = {{"Rep", "X1", "", "Repère du bornier", false},
                 {"Nb", "6", "", "Nombre de bornes (1 à 24)", false},
                 {"Debut", "1", "", "Numéro de la première borne", false}};
    }
    QString prefix() const override { return "X"; }
    QString kind() const override { return "terminal"; }
    QString title() const override { return "Bornier"; }
    int pinCount() const override { return 2 * CAP; }
    QPointF pinLocal(int i) const override { return QPointF((i % CAP) * 40, i < CAP ? -40 : 40); }
    bool pinActive(int i) const override { return (i % CAP) < count(); }
    QList<QPair<int, int>> internalLinks() const override
    {
        QList<QPair<int, int>> l;
        for (int k = 0; k < count(); ++k)
            l.append({k, CAP + k});
        return l;
    }
    QRectF boundingRect() const override { return bodyRect().adjusted(-20, -20, 20, 60); }
protected:
    int count() const { return qBound(1, props[1].value.trimmed().toInt(), CAP); }
    QRectF bodyRect() const override { return QRectF(-20, -44, (count() - 1) * 40 + 40, 88); }
    void drawBody(QPainter *p) const override
    {
        const int start = props[2].value.trimmed().toInt();
        for (int k = 0; k < count(); ++k) {
            const qreal x = k * 40;
            p->drawLine(QPointF(x, -40), QPointF(x, -16));
            p->drawLine(QPointF(x, 16), QPointF(x, 40));
            p->drawRect(QRectF(x - 16, -16, 32, 32));
            p->drawEllipse(QPointF(x, -10), 3, 3);
            p->drawEllipse(QPointF(x, 10), 3, 3);
            upright(p, this, QPointF(x, 0), QString("%1.%2").arg(props[0].value).arg(start + k), 8);
        }
    }
};

// ---------- Automate programmable (sans marque) ----------
class PlcItem : public SymbolItem
{
public:
    static constexpr int CAP_DI = 32, CAP_DO = 32, CAP_AI = 8, CAP_AO = 4;
    static constexpr int I0 = 2, O0 = I0 + CAP_DI, A0 = O0 + CAP_DO, B0 = A0 + CAP_AI,
        CI = B0 + CAP_AO, CO = CI + 1;

    PlcItem()
    {
        props = {{"Nom", "Automate", "", "Désignation (libre)", false},
                 {"DI", "8", "", "Entrées TOR (0 à 32)", false},
                 {"DO", "8", "", "Sorties TOR (0 à 32)", false},
                 {"AI", "0", "", "Entrées analogiques (0 à 8)", false},
                 {"AO", "0", "", "Sorties analogiques (0 à 4)", false}};
    }
    QString prefix() const override { return "A"; }
    QString kind() const override { return "plc"; }
    QString title() const override { return "Automate programmable"; }
    int pinCount() const override { return CO + 1; }     // 0 +24V, 1 0V, entrées, sorties, COM
    QPointF pinLocal(int i) const override
    {
        if (i == 0) return QPointF(-60, -20);
        if (i == 1) return QPointF(-60, 20);
        if (i < O0) return QPointF((i - I0) * 40, -80);
        if (i < A0) return QPointF((i - O0) * 40, 80);
        if (i < B0) return QPointF((nDI() + (i - A0)) * 40, -80);
        if (i < CI) return QPointF((nDO() + (i - B0)) * 40, 80);
        return QPointF(right() + 30, i == CI ? -20 : 20);
    }
    bool pinActive(int i) const override
    {
        if (i < I0) return true;
        if (i < O0) return i - I0 < nDI();
        if (i < A0) return i - O0 < nDO();
        if (i < B0) return i - A0 < nAI();
        if (i < CI) return i - B0 < nAO();
        return true;
    }
    QRectF boundingRect() const override { return bodyRect().adjusted(-20, -10, 20, 60); }
protected:
    int cnt(int idx, int cap) const { return qBound(0, props[idx].value.trimmed().toInt(), cap); }
    int nDI() const { return cnt(1, CAP_DI); }
    int nDO() const { return cnt(2, CAP_DO); }
    int nAI() const { return cnt(3, CAP_AI); }
    int nAO() const { return cnt(4, CAP_AO); }
    int cols() const { return qMax(1, qMax(nDI() + nAI(), nDO() + nAO())); }
    qreal right() const { return (cols() - 1) * 40 + 30; }
    QRectF bodyRect() const override { return QRectF(-66, -84, right() + 66 + 36, 168); }

    void drawBody(QPainter *p) const override
    {
        const qreal R = right();
        p->drawRect(QRectF(-30, -60, R + 30, 120));
        upright(p, this, QPointF((R - 30) / 2, -4), props[0].value, 12);

        auto top = [&](int idx, const QString &lab) {
            const qreal x = idx * 40;
            p->drawLine(QPointF(x, -80), QPointF(x, -60));
            upright(p, this, QPointF(x, -50), lab, 8);
        };
        auto bottom = [&](int idx, const QString &lab) {
            const qreal x = idx * 40;
            p->drawLine(QPointF(x, 80), QPointF(x, 60));
            upright(p, this, QPointF(x, 50), lab, 8);
        };
        for (int k = 0; k < nDI(); ++k) top(k, QString("I%1").arg(k));
        for (int k = 0; k < nAI(); ++k) top(nDI() + k, QString("AI%1").arg(k));
        for (int k = 0; k < nDO(); ++k) bottom(k, QString("Q%1").arg(k));
        for (int k = 0; k < nAO(); ++k) bottom(nDO() + k, QString("AO%1").arg(k));

        for (int y : {-20, 20}) {
            p->drawLine(QPointF(-60, y), QPointF(-30, y));
            p->drawLine(QPointF(R + 30, y), QPointF(R, y));
            upright(p, this, QPointF(R - 18, y), "COM", 8);
        }
        upright(p, this, QPointF(0, -20), "24V", 8);
        upright(p, this, QPointF(0, 20), "0V", 8);
    }
};

} // namespace

SymbolItem *makeElectroSymbol2(const QString &k)
{
    // appareils à pôles : « mcb_3p »...
    static const QRegularExpression re("^([a-z]+)_([1-4])p$");
    const QRegularExpressionMatch m = re.match(k);
    if (m.hasMatch()) {
        const PoleType *t = poleType(m.captured(1));
        const int n = m.captured(2).toInt();
        if (t && (t->mask & (1 << n)))
            return new PoleDevice(*t, n);
    }

    if (k == "earth")  return new EarthItem;
    if (k == "psu")    return new PsuItem;
    if (k == "contact_co") return new ChangeoverItem;
    if (k == "motor1") return new MotorRound(false);
    if (k == "motordc") return new MotorRound(true);
    if (k == "plc")    return new PlcItem;
    if (k == "terminal") return new TerminalItem;
    if (k == "sens_ind") return new Sensor3Item(k, "Capteur inductif", "I");
    if (k == "sens_cap") return new Sensor3Item(k, "Capteur capacitif", "C");
    if (k == "sens_pho") return new Sensor3Item(k, "Capteur photoélectrique", "P");

    auto contact = [](const QString &kind, const QString &title, const QString &prefix, bool nc,
                      int decor, bool delay, const QList<Prop> &pr, bool showRef) -> SymbolItem * {
        auto *s = new V2Item(kind, title, prefix, [=](QPainter *p, const SymbolItem *it) {
            drawContact(p, it, nc, decor, delay);
        });
        s->props = pr;
        s->showRef = showRef;
        return s;
    };
    const QList<Prop> td = {{"Bobine", "KT1", "", "Relais temporisé qui commande ce contact", true},
                            {"Bornes", "", "", "Repères des bornes", true}};
    auto withBornes = [](QList<Prop> p, const QString &b) { p[1].value = b; return p; };

    if (k == "contact_td_no") return contact(k, "Contact NO temporisé", "KC", false, 0, true,
                       withBornes(td, "67-68"), false);
    if (k == "contact_td_nc") return contact(k, "Contact NC temporisé", "KC", true, 0, true,
                       withBornes(td, "55-56"), false);
    const QList<Prop> noB = {{"Bornes", "13-14", "", "Repères des bornes", true}};
    const QList<Prop> ncB = {{"Bornes", "11-12", "", "Repères des bornes", true}};
    if (k == "limit_no") return contact(k, "Fin de course NO", "SQ", false, 1, false, noB, true);
    if (k == "limit_nc") return contact(k, "Fin de course NC", "SQ", true, 1, false, ncB, true);
    if (k == "level_no") return contact(k, "Détecteur de niveau", "SL", false, 2, false, noB, true);
    if (k == "press_no") return contact(k, "Pressostat", "SP", false, 3, false, noB, true);
    if (k == "therm_no") return contact(k, "Thermostat", "ST", false, 4, false, noB, true);

    if (k == "timer_coil") {
        auto *s = new V2Item(k, "Relais temporisé (bobine)", "KT", [](QPainter *p, const SymbolItem *it) {
            p->drawLine(QPointF(0, -40), QPointF(0, -20));
            p->drawLine(QPointF(0, 20), QPointF(0, 40));
            p->drawRect(QRectF(-14, -20, 28, 40));
            p->drawPolygon(QPolygonF({QPointF(-6, -9), QPointF(6, -9), QPointF(0, 0)}));
            p->drawPolygon(QPolygonF({QPointF(-6, 9), QPointF(6, 9), QPointF(0, 0)}));
            upright(p, it, QPointF(-1, -31), "A1", 9);
            upright(p, it, QPointF(-1, 31), "A2", 9);
        }, QRectF(-18, -44, 36, 88));
        s->props = {{"U", "24", "V", "Tension de la bobine", true},
                    {"Tempo", "5", "s", "Temporisation", true},
                    {"Type", "travail", "", "À l'appel (travail) ou à la chute (repos)", false}};
        return s;
    }
    if (k == "valve") {
        auto *s = new V2Item(k, "Électrovanne", "YV", [](QPainter *p, const SymbolItem *) {
            p->drawLine(QPointF(0, -40), QPointF(0, -20));
            p->drawLine(QPointF(0, 20), QPointF(0, 40));
            p->drawRect(QRectF(-14, -20, 28, 40));
            p->drawPolygon(QPolygonF({QPointF(-8, -12), QPointF(8, -12), QPointF(0, 0)}));
            p->drawPolygon(QPolygonF({QPointF(-8, 12), QPointF(8, 12), QPointF(0, 0)}));
        }, QRectF(-18, -44, 36, 88));
        s->props = {{"U", "24", "V", "Tension de commande", true}};
        return s;
    }
    if (k == "heater") {
        auto *s = new V2Item(k, "Résistance chauffante", "RC", [](QPainter *p, const SymbolItem *) {
            p->drawLine(QPointF(0, -40), QPointF(0, -20));
            p->drawLine(QPointF(0, 20), QPointF(0, 40));
            p->drawRect(QRectF(-8, -20, 16, 40));
            p->drawLine(QPointF(-8, 20), QPointF(8, -20));
        }, QRectF(-18, -44, 36, 88));
        s->props = {{"P", "1000", "W", "Puissance", true}};
        return s;
    }
    if (k == "estop") {
        auto *s = new V2Item(k, "Arrêt d'urgence", "S", [](QPainter *p, const SymbolItem *it) {
            drawContact(p, it, true, 0, false);
            dashed(p, QLineF(-6, 0, 12, 0));
            p->save();
            p->setBrush(QColor(200, 0, 0));
            p->drawEllipse(QPointF(18, 0), 6, 6);
            p->restore();
        });
        s->props = {{"Bornes", "11-12", "", "Repères des bornes", true}};
        return s;
    }
    if (k == "selector") {
        auto *s = new V2Item(k, "Commutateur à clé / sélecteur", "S", [](QPainter *p, const SymbolItem *it) {
            drawContact(p, it, false, 0, false);
            dashed(p, QLineF(-6, 0, 12, 0));
            p->drawEllipse(QPointF(16, 0), 4, 4);
        });
        s->props = {{"Bornes", "13-14", "", "Repères des bornes", true}};
        return s;
    }
    if (k == "buzzer") {
        auto *s = new V2Item(k, "Avertisseur sonore", "H", [](QPainter *p, const SymbolItem *) {
            p->drawLine(QPointF(0, -40), QPointF(0, -14));
            p->drawLine(QPointF(0, 0), QPointF(0, 40));
            p->drawChord(QRectF(-14, -14, 28, 28), 0, 180 * 16);
        }, QRectF(-18, -44, 36, 88));
        s->props = {{"U", "24", "V", "Tension", true}};
        return s;
    }
    return nullptr;
}