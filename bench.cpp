#include "bench.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QKeyEvent>
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QComboBox>
#include <QAction>
#include <QShowEvent>
#include <QWheelEvent>
#include <QGraphicsSimpleTextItem>
#include <QtMath>
#include <algorithm>

static const double PI2 = 6.283185307179586;

// =====================================================================================
//  Douille
// =====================================================================================
BenchSocket::BenchSocket(BenchModule *m, const QString &n, const QColor &c, QPointF pos)
    : QGraphicsEllipseItem(-9, -9, 18, 18, m), module(m), name(n), col(c)
{
    setPos(pos);
    setZValue(2);
    setToolTip(n);
}

void BenchSocket::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(QPen(QColor(30, 30, 30), 1.5));
    p->setBrush(col);
    p->drawEllipse(QPointF(0, 0), 9, 9);
    if (col == QColor(230, 200, 20)) {                 // PE : jaune / vert
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(40, 150, 60));
        p->drawRect(QRectF(-9, -2, 18, 4));
        p->setPen(QPen(QColor(30, 30, 30), 1.5));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(QPointF(0, 0), 9, 9);
    }
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(15, 15, 15));
    p->drawEllipse(QPointF(0, 0), 3.6, 3.6);
}

// =====================================================================================
//  Fil
// =====================================================================================
BenchWire::BenchWire(BenchSocket *a_, BenchSocket *b_, const QColor &col) : a(a_), b(b_)
{
    setPen(QPen(col, 4, Qt::SolidLine, Qt::RoundCap));
    setZValue(10);
    setFlag(ItemIsSelectable);
    a->wires.push_back(this);
    b->wires.push_back(this);
    updatePath();
}

BenchWire::~BenchWire()
{
    for (BenchSocket *s : {a, b}) {
        auto &v = s->wires;
        v.erase(std::remove(v.begin(), v.end(), this), v.end());
    }
}

void BenchWire::updatePath()
{
    QPointF p1 = a->center(), p2 = b->center();
    double sag = 40 + 0.25 * QLineF(p1, p2).length();
    QPainterPath path(p1);
    path.cubicTo(p1 + QPointF(0, sag), p2 + QPointF(0, sag), p2);
    setPath(path);
}

// =====================================================================================
//  BenchModule
// =====================================================================================
BenchModule::BenchModule(const QString &t, int w_, int h_, const QColor &c) : title(t), w(w_), h(h_), body(c)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(1);
}

BenchSocket *BenchModule::addSocket(const QString &name, const QColor &col, double x, double y)
{
    auto *s = new BenchSocket(this, name, col, QPointF(x, y));
    sockets.push_back(s);
    return s;
}

void BenchModule::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing);
    QColor border = !warn.isEmpty() ? QColor(210, 40, 40) : (isSelected() ? QColor(40, 110, 220) : QColor(70, 70, 70));
    p->setPen(QPen(border, !warn.isEmpty() || isSelected() ? 3 : 1.5));
    p->setBrush(body);
    p->drawRoundedRect(QRectF(1, 1, w - 2, h - 2), 8, 8);

    QColor txt = body.lightness() < 120 ? QColor(240, 240, 240) : QColor(25, 25, 25);
    p->setPen(txt);
    QFont f = p->font(); f.setPointSize(9); f.setBold(true); p->setFont(f);
    p->drawText(QRectF(6, 4, w - 12, 18), Qt::AlignCenter, title);

    f.setBold(false); f.setPointSize(7); p->setFont(f);
    for (BenchSocket *s : sockets)
        p->drawText(QRectF(s->pos().x() - 24, s->pos().y() + 11, 48, 12), Qt::AlignCenter, s->name);

    paintExtra(p);

    if (!warn.isEmpty()) {
        f.setBold(true); f.setPointSize(8); p->setFont(f);
        p->setPen(QColor(210, 40, 40));
        p->drawText(QRectF(4, h - 16, w - 8, 14), Qt::AlignCenter, warn);
    }
}

QVariant BenchModule::itemChange(GraphicsItemChange c, const QVariant &v)
{
    if (c == ItemPositionChange) {                      // aimantation sur une grille de 10
        const QPointF p = v.toPointF();
        return QPointF(qRound(p.x() / 10.0) * 10, qRound(p.y() / 10.0) * 10);
    }
    if (c == ItemPositionHasChanged)
        for (BenchSocket *s : sockets)
            for (BenchWire *wi : s->wires) wi->updatePath();
    return QGraphicsItem::itemChange(c, v);
}

void BenchModule::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *)
{
    if (params.empty()) return;
    QDialog d;
    d.setWindowTitle(title);
    auto *form = new QFormLayout(&d);
    std::vector<QDoubleSpinBox *> boxes;
    for (const BenchParam &pr : params) {
        auto *sb = new QDoubleSpinBox;
        sb->setDecimals(pr.decimals);
        sb->setRange(pr.min, pr.max);
        sb->setSingleStep(pr.step);
        sb->setValue(pr.value);
        if (!pr.unit.isEmpty()) sb->setSuffix(" " + pr.unit);
        form->addRow(pr.name, sb);
        boxes.push_back(sb);
    }
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    if (d.exec() != QDialog::Accepted) return;
    for (size_t i = 0; i < params.size(); ++i) params[i].value = boxes[i]->value();
    update();
    if (changed) changed();
}

void BenchModule::wheelEvent(QGraphicsSceneWheelEvent *e)
{
    if (wheelParam < 0 || wheelParam >= (int)params.size()) { e->ignore(); return; }
    BenchParam &p = params[wheelParam];
    p.value = qBound(p.min, p.value + (e->delta() > 0 ? p.step : -p.step), p.max);
    update();
    if (changed) changed();
    e->accept();
}

void BenchView::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        const double f = e->angleDelta().y() > 0 ? 1.15 : 1 / 1.15;
        setTransformationAnchor(AnchorUnderMouse);
        scale(f, f);
        e->accept();
        return;
    }
    QGraphicsView::wheelEvent(e);
}

// =====================================================================================
//  Scene
// =====================================================================================
BenchSocket *BenchScene::socketAt(QPointF p) const
{
    for (QGraphicsItem *it : items(p))
        if (auto *s = dynamic_cast<BenchSocket *>(it)) return s;
    return nullptr;
}

void BenchScene::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        if (BenchSocket *s = socketAt(e->scenePos())) {
            from = s;
            temp = addLine(QLineF(s->center(), e->scenePos()), QPen(QColor(90, 90, 90), 3, Qt::DashLine));
            temp->setZValue(20);
            e->accept();
            return;
        }
    } else if (e->button() == Qt::RightButton) {
        for (QGraphicsItem *it : items(e->scenePos()))
            if (auto *w = dynamic_cast<BenchWire *>(it)) { if (onDeleteWire) onDeleteWire(w); e->accept(); return; }
    }
    QGraphicsScene::mousePressEvent(e);
}

void BenchScene::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    if (from && temp) { temp->setLine(QLineF(from->center(), e->scenePos())); return; }
    QGraphicsScene::mouseMoveEvent(e);
}

void BenchScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    if (from) {
        BenchSocket *to = socketAt(e->scenePos());
        BenchSocket *f = from;
        from = nullptr;
        if (temp) { removeItem(temp); delete temp; temp = nullptr; }
        if (to && to != f && onConnect) onConnect(f, to);
        return;
    }
    QGraphicsScene::mouseReleaseEvent(e);
}

void BenchScene::keyPressEvent(QKeyEvent *e)
{
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && onDeleteSelection) {
        onDeleteSelection();
        return;
    }
    QGraphicsScene::keyPressEvent(e);
}

// =====================================================================================
//  Modules concrets
// =====================================================================================
namespace {

const QColor cRed(190, 30, 35), cBlack(35, 35, 38), cBlue(40, 90, 200), cPE(230, 200, 20), cBrown(120, 70, 35);

QString fmt(double x, int d = 2) { return QString::number(x, 'f', d); }

// ---- Pupitre : sectionneur + arret d'urgence + sortie 400 V + N -------------------------
class Pupitre : public BenchModule {
public:
    Pupitre() : BenchModule(QStringLiteral("Pupitre d'alimentation"), 330, 140, QColor(200, 203, 208)) {
        pe = addSocket("PE", cPE, 45, 100);
        n  = addSocket("N", cBlue, 105, 100);
        p1 = addSocket("PH1", cBrown, 165, 100);
        p2 = addSocket("PH2", cBlack, 225, 100);
        p3 = addSocket("PH3", QColor(140, 140, 145), 285, 100);
        params = { {"Sectionneur (0 = ouvert, 1 = ferme)", "", 0, 1, 1, 1, 0},
                   {"Arret d'urgence (1 = enfonce)", "", 0, 1, 0, 1, 0},
                   {"Tension du reseau variable", "%", 0, 100, 100, 5, 0} };
        wheelParam = 2;
        instrument = true;
    }
    bool isSupply() const override { return true; }
    bool live() const { return params[0].value > 0.5 && params[1].value < 0.5; }
    double vph() const { return 230.0 * params[2].value / 100.0; }
    void sources(std::vector<BenchSrc> &s) override {
        if (!live()) return;
        double v = vph();
        s.push_back({p1, n, std::polar(v, 0.0)});
        s.push_back({p2, n, std::polar(v, -PI2 / 3)});
        s.push_back({p3, n, std::polar(v, -2 * PI2 / 3)});
    }
    void paintExtra(QPainter *p) override {
        p->setPen(Qt::NoPen);
        p->setBrush(live() ? QColor(40, 180, 80) : QColor(120, 120, 120));
        p->drawEllipse(QPointF(30, 45), 9, 9);
        p->setBrush(params[1].value > 0.5 ? QColor(200, 30, 30) : QColor(120, 40, 40));
        p->drawEllipse(QPointF(70, 45), 12, 12);
        p->setPen(QColor(30, 30, 30));
        QFont f = p->font(); f.setPointSize(8); p->setFont(f);
        p->drawText(QRectF(100, 30, 220, 30), Qt::AlignVCenter | Qt::AlignLeft,
                    live() ? QStringLiteral("EN SERVICE - %1 V / phase").arg(fmt(vph(), 0))
                           : QStringLiteral("HORS TENSION"));
    }
    BenchSocket *pe, *n, *p1, *p2, *p3;
};

// ---- Rheostat (3 douilles + PE) -----------------------------------------------------------
class Rheostat : public BenchModule {
public:
    Rheostat(double r, double imax) : BenchModule(QStringLiteral("Rheostat %1 ohm").arg(r, 0, 'f', 0), 200, 150, QColor(95, 150, 195)) {
        c = addSocket("C (curseur)", cBlack, 100, 45);
        a = addSocket("A", cBlack, 45, 110);
        b = addSocket("B", cRed, 100, 110);
        pe = addSocket("PE", cPE, 155, 110);
        params = { {"Resistance totale", "ohm", 1, 100000, r, 10, 0},
                   {"Courant admissible", "A", 0.01, 50, imax, 0.1, 2},
                   {"Position du curseur", "%", 0, 100, 50, 2, 0} };
        wheelParam = 2;
    }
    double rac() const { return std::max(1e-3, params[0].value * params[2].value / 100.0); }
    double rcb() const { return std::max(1e-3, params[0].value * (1 - params[2].value / 100.0)); }
    void elements(std::vector<BenchElem> &e) override {
        e.push_back({a, c, 1.0 / rac()});
        e.push_back({c, b, 1.0 / rcb()});
    }
    void post(const BenchSolution &s) override {
        double i1 = std::abs((s.at(a) - s.at(c)) / rac());
        double i2 = std::abs((s.at(c) - s.at(b)) / rcb());
        i = std::max(i1, i2);
        warn = i > params[1].value * 1.001 ? QStringLiteral("SURCHARGE %1 A").arg(fmt(i, 2)) : QString();
    }
    void paintExtra(QPainter *p) override {
        p->setPen(QColor(20, 20, 20));
        p->drawText(QRectF(6, 70, 188, 14), Qt::AlignCenter,
                    QStringLiteral("%1 ohm  -  curseur %2 %  -  %3 A max")
                        .arg(fmt(params[0].value, 0)).arg(fmt(params[2].value, 0)).arg(fmt(params[1].value, 2)));
    }
    BenchSocket *a, *b, *c, *pe;
    double i = 0;
};

// ---- Boite a decades (Langlois R50) -------------------------------------------------------------
class Decades : public BenchModule {
public:
    Decades() : BenchModule(QStringLiteral("Boite a decades R50"), 340, 120, QColor(205, 208, 212)) {
        a = addSocket("", cBlack, 130, 85);
        b = addSocket("", cBlack, 210, 85);
        const char *names[5] = {"x 10 kohm (7 mA)", "x 1 kohm (20 mA)", "x 100 ohm (70 mA)", "x 10 ohm (200 mA)", "x 1 ohm (700 mA)"};
        for (const char *n : names) params.push_back({QString::fromUtf8(n), "", 0, 10, 0, 1, 0});
    }
    static double weight(int k) { static const double w[5] = {10000, 1000, 100, 10, 1}; return w[k]; }
    static double imaxOf(int k) { static const double m[5] = {0.007, 0.020, 0.070, 0.200, 0.700}; return m[k]; }
    double r() const {
        double s = 0;
        for (int k = 0; k < 5; ++k) s += params[k].value * weight(k);
        return std::max(s, 1e-3);
    }
    double limit() const {
        double m = 1e9;
        for (int k = 0; k < 5; ++k) if (params[k].value > 0.5) m = std::min(m, imaxOf(k));
        return m;
    }
    void elements(std::vector<BenchElem> &e) override { e.push_back({a, b, 1.0 / r()}); }
    void post(const BenchSolution &s) override {
        double i = std::abs((s.at(a) - s.at(b)) / r());
        warn = (i > limit() * 1.001 && limit() < 1e8) ? QStringLiteral("SURCHARGE %1 mA").arg(fmt(i * 1000, 1)) : QString();
    }
    void paintExtra(QPainter *p) override {
        p->setPen(QColor(20, 20, 20));
        p->drawText(QRectF(6, 30, 328, 16), Qt::AlignCenter, QStringLiteral("R = %1 ohm").arg(fmt(r(), 0)));
    }
    BenchSocket *a, *b;
};

// ---- Charge inductive (un bobinage : L serie R) ----------------------------------------------
class Inductor : public BenchModule {
public:
    Inductor() : BenchModule(QStringLiteral("Charge inductive (1 bobinage)"), 220, 120, QColor(160, 148, 130)) {
        a = addSocket("E", cBlack, 70, 85);
        b = addSocket("S", cBlack, 150, 85);
        params = { {"Inductance L", "H", 0.019, 0.856, 0.214, 0.01, 3},
                   {"Resistance du cuivre", "ohm", 0, 20, 0.46, 0.1, 2},
                   {"Courant permanent", "A", 0.1, 50, 13, 0.5, 1} };
    }
    bcplx z() const { return bcplx(params[1].value, PI2 * BenchWindow::kFreq * params[0].value); }
    void elements(std::vector<BenchElem> &e) override { e.push_back({a, b, 1.0 / z()}); }
    void post(const BenchSolution &s) override {
        double i = std::abs((s.at(a) - s.at(b)) / z());
        warn = i > params[2].value * 1.001 ? QStringLiteral("SURCHARGE %1 A").arg(fmt(i, 1)) : QString();
    }
    void paintExtra(QPainter *p) override {
        p->setPen(QColor(20, 20, 20));
        p->drawText(QRectF(6, 30, 208, 16), Qt::AlignCenter,
                    QStringLiteral("L = %1 H   X = %2 ohm").arg(fmt(params[0].value, 3)).arg(fmt(z().imag(), 1)));
    }
    BenchSocket *a, *b;
};

// ---- Trois condensateurs ---------------------------------------------------------------------
class Capas : public BenchModule {
public:
    Capas() : BenchModule(QStringLiteral("Condensateurs (3 x 5 uF)"), 300, 140, QColor(215, 217, 220)) {
        for (int k = 0; k < 3; ++k) {
            s[k][0] = addSocket(QStringLiteral("C%1+").arg(k + 1), cBlack, 50 + 100 * k, 50);
            s[k][1] = addSocket(QStringLiteral("C%1-").arg(k + 1), cRed, 50 + 100 * k, 105);
            params.push_back({QStringLiteral("Capacite C%1").arg(k + 1), "uF", 0.1, 100, 5, 0.5, 1});
        }
    }
    void elements(std::vector<BenchElem> &e) override {
        for (int k = 0; k < 3; ++k)
            e.push_back({s[k][0], s[k][1], bcplx(0, PI2 * BenchWindow::kFreq * params[k].value * 1e-6)});
    }
    BenchSocket *s[3][2];
};

// ---- Lampe ----------------------------------------------------------------------------------------
class Lamp : public BenchModule {
public:
    Lamp() : BenchModule(QStringLiteral("Lampe a incandescence"), 150, 140, QColor(225, 227, 230)) {
        a = addSocket("1", cRed, 45, 105);
        b = addSocket("2", cRed, 105, 105);
        params = { {"Puissance nominale", "W", 1, 2000, 60, 5, 0}, {"Tension nominale", "V", 1, 400, 230, 5, 0} };
    }
    double r() const { return params[1].value * params[1].value / params[0].value; }
    void elements(std::vector<BenchElem> &e) override { e.push_back({a, b, 1.0 / r()}); }
    void post(const BenchSolution &s) override { u = std::abs(s.at(a) - s.at(b)); }
    void paintExtra(QPainter *p) override {
        double glow = std::min(1.0, std::pow(u / params[1].value, 2));
        p->setPen(QPen(QColor(60, 60, 60), 1.5));
        p->setBrush(QColor(255, int(240 - 40 * glow), int(210 - 160 * glow)));
        p->drawEllipse(QPointF(75, 58), 24, 24);
        p->drawRect(QRectF(65, 80, 20, 12));
    }
    BenchSocket *a, *b;
    double u = 0;
};

// ---- Pince multifonction (modele generique) ---------------------------------------------------------------
class Pince : public BenchModule {
public:
    Pince() : BenchModule(QStringLiteral("Pince multifonction"), 330, 200, QColor(45, 48, 55)) {
        instrument = true;
        v    = addSocket("V", cRed, 40, 160);
        com  = addSocket("COM", cBlack, 95, 160);
        iin  = addSocket("I entree", cRed, 155, 160);
        iout = addSocket("I sortie", cBlack, 210, 160);
        so   = addSocket("Sortie osc.", QColor(150, 150, 155), 285, 160);
        params = { {"Sensibilite de la sortie oscillo", "V/A", 0.001, 10, 0.1, 0.05, 3} };
        so->source = [this](const BenchSolution &s) { return (s.at(iin) - s.at(iout)) / kShunt * params[0].value; };
    }
    void elements(std::vector<BenchElem> &e) override { e.push_back({iin, iout, 1.0 / kShunt}); }   // pince = 1 mohm serie
    void post(const BenchSolution &s) override {
        U = s.at(v) - s.at(com);
        I = (s.at(iin) - s.at(iout)) / kShunt;
        S = U * std::conj(I);
    }
    void paintExtra(QPainter *p) override {
        p->setPen(QPen(QColor(10, 10, 10), 2));
        p->setBrush(QColor(150, 175, 140));
        p->drawRoundedRect(QRectF(15, 30, 300, 110), 5, 5);
        double sa = std::abs(S);
        double cosphi = sa > 1e-9 ? S.real() / sa : 0;
        QFont f("Monospace"); f.setStyleHint(QFont::TypeWriter); f.setPointSize(9); p->setFont(f);
        p->setPen(QColor(20, 30, 15));
        QStringList l = { QStringLiteral("U   = %1 V").arg(fmt(std::abs(U), 1)),
                          QStringLiteral("I   = %1 A").arg(fmt(std::abs(I), 3)),
                          QStringLiteral("P   = %1 W").arg(fmt(S.real(), 1)),
                          QStringLiteral("Q   = %1 var").arg(fmt(S.imag(), 1)),
                          QStringLiteral("S   = %1 VA").arg(fmt(sa, 1)),
                          QStringLiteral("cos = %1  (%2)").arg(fmt(cosphi, 2)).arg(S.imag() > 0.05 ? "ind" : (S.imag() < -0.05 ? "cap" : "")) };
        for (int k = 0; k < l.size(); ++k) p->drawText(QPointF(25, 50 + 16 * k), l[k]);
    }
    static constexpr double kShunt = 1e-3;
    BenchSocket *v, *com, *iin, *iout, *so;
    bcplx U{0, 0}, I{0, 0}, S{0, 0};
};

// ---- Sonde differentielle (rapport d'attenuation reglable) ---------------------------------------
class Probe : public BenchModule {
public:
    Probe() : BenchModule(QStringLiteral("Sonde differentielle"), 210, 150, QColor(60, 64, 70)) {
        instrument = true;
        p   = addSocket("+", cRed, 45, 100);
        m   = addSocket("-", cBlack, 105, 100);
        out = addSocket("Sortie", QColor(150, 150, 155), 165, 100);
        params = { {"Rapport d'attenuation (V/V)", "", 1, 2000, 100, 10, 0} };
        out->source = [this](const BenchSolution &s) { return (s.at(p) - s.at(m)) / params[0].value; };
    }
    void paintExtra(QPainter *g) override {
        g->setPen(QColor(235, 235, 235));
        g->drawText(QRectF(6, 36, w - 12, 16), Qt::AlignCenter, QStringLiteral("1 / %1").arg(fmt(params[0].value, 0)));
    }
    BenchSocket *p, *m, *out;
};

// ---- Oscilloscope 2 voies ------------------------------------------------------------------------
class Scope : public BenchModule {
public:
    Scope() : BenchModule(QStringLiteral("Oscilloscope 2 voies"), 440, 420, QColor(70, 74, 82)) {
        instrument = true;
        c1  = addSocket("CH1", QColor(235, 205, 60), 90, 394);
        c2  = addSocket("CH2", QColor(60, 200, 220), 180, 394);
        gnd = addSocket("Masse", cBlack, 270, 394);
        params = { {"CH1 : volts par division", "V", 0.001, 1000, 1, 0.5, 3},
                   {"CH2 : volts par division", "V", 0.001, 1000, 0.1, 0.05, 3},
                   {"Base de temps", "ms/div", 0.1, 100, 5, 1, 1} };
        wheelParam = 2;
    }
    bool on1() const { return !c1->wires.empty(); }
    bool on2() const { return !c2->wires.empty(); }
    bcplx read(const BenchSolution &s, BenchSocket *c) const {
        bcplx ref = gnd->wires.empty() ? bcplx(0, 0) : s.at(gnd);
        bcplx v = s.at(c) - ref;
        for (BenchSocket *t : s.all)
            if (t != c && t->net == c->net && t->source) v += t->source(s);
        return v;
    }
    void post(const BenchSolution &s) override { v1 = read(s, c1); v2 = read(s, c2); }
    static QPainterPath trace(const bcplx &v, double vdiv, double msdiv, double t0) {
        QPainterPath path;
        const double w = PI2 * BenchWindow::kFreq;
        for (int px = 0; px <= 400; px += 2) {
            double t = msdiv * 1e-3 * 10.0 * px / 400.0 + t0;
            double val = std::real(std::sqrt(2.0) * v * std::polar(1.0, w * t));
            double y = 188 - val / vdiv * 40.0;
            y = std::max(20.0, std::min(356.0, y));
            if (px == 0) path.moveTo(20 + px, y); else path.lineTo(20 + px, y);
        }
        return path;
    }
    void paintExtra(QPainter *p) override {
        const QRectF scr(20, 28, 400, 320);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(12, 22, 16));
        p->drawRect(scr);
        p->setPen(QPen(QColor(55, 85, 65), 1));
        for (int i = 0; i <= 10; ++i) p->drawLine(QPointF(20 + 40 * i, 28), QPointF(20 + 40 * i, 348));
        for (int j = 0; j <= 8; ++j)  p->drawLine(QPointF(20, 28 + 40 * j), QPointF(420, 28 + 40 * j));
        p->setPen(QPen(QColor(90, 130, 100), 1.5));
        p->drawLine(QPointF(20, 188), QPointF(420, 188));
        p->drawLine(QPointF(220, 28), QPointF(220, 348));

        const double w = PI2 * BenchWindow::kFreq;
        bool a1 = on1() && std::abs(v1) > 1e-9, a2 = on2() && std::abs(v2) > 1e-9;
        double ph = a1 ? std::arg(v1) : (a2 ? std::arg(v2) : 0.0);
        double t0 = (-PI2 / 4 - ph) / w;                 // declenchement : front montant de la voie 1
        p->save();
        p->setClipRect(scr);
        p->setRenderHint(QPainter::Antialiasing);
        p->setBrush(Qt::NoBrush);                        // sinon le trace est rempli et masque l'autre voie
        if (on1()) { p->setPen(QPen(QColor(235, 205, 60), 2)); p->drawPath(trace(v1, params[0].value, params[2].value, t0)); }
        if (on2()) { p->setPen(QPen(QColor(60, 200, 220), 2)); p->drawPath(trace(v2, params[1].value, params[2].value, t0)); }
        p->restore();

        QFont f = p->font(); f.setPointSize(8); f.setBold(false); p->setFont(f);
        p->setPen(QColor(235, 205, 60));
        p->drawText(QPointF(24, 361), QStringLiteral("CH1 %1 V/div  Veff %2 V").arg(fmt(params[0].value, 3)).arg(fmt(std::abs(v1), 3)));
        p->setPen(QColor(60, 200, 220));
        p->drawText(QPointF(24, 375), QStringLiteral("CH2 %1 V/div  Veff %2 V").arg(fmt(params[1].value, 3)).arg(fmt(std::abs(v2), 3)));
        p->setPen(QColor(230, 230, 230));
        QString dphi = (a1 && a2) ? fmt(std::arg(v2 / v1) * 180.0 / 3.141592653589793, 1) + QStringLiteral(" deg") : QStringLiteral("-");
        p->drawText(QPointF(250, 361), QStringLiteral("%1 ms/div").arg(fmt(params[2].value, 1)));
        p->drawText(QPointF(250, 375), QStringLiteral("Dephasage CH2/CH1 : ") + dphi);
    }
    BenchSocket *c1, *c2, *gnd;
    bcplx v1{0, 0}, v2{0, 0};
};

} // namespace

// =====================================================================================
//  Fenetre
// =====================================================================================
QStringList BenchWindow::kinds()
{
    return { "Pupitre d'alimentation", "Rheostat 1400 ohm / 1 A", "Rheostat 2000 ohm / 0,2 A",
             "Boite a decades R50", "Charge inductive", "Condensateurs 3 x 5 uF",
             "Lampe a incandescence", "Pince multifonction", "Oscilloscope 2 voies", "Sonde differentielle" };
}

BenchModule *BenchWindow::addModule(const QString &kind, QPointF pos)
{
    BenchModule *m = nullptr;
    int i = kinds().indexOf(kind);
    switch (i) {
    case 0: m = new Pupitre; break;
    case 1: m = new Rheostat(1400, 1.0); break;
    case 2: m = new Rheostat(2000, 0.2); break;
    case 3: m = new Decades; break;
    case 4: m = new Inductor; break;
    case 5: m = new Capas; break;
    case 6: m = new Lamp; break;
    case 7: m = new Pince; break;
    case 8: m = new Scope; break;
    case 9: m = new Probe; break;
    default: return nullptr;
    }
    m->kind = i;
    pos = (pos.x() < -1e8) ? findSlot(m) : pos;
    m->setPos(pos);
    m->changed = [this] { recompute(); };
    m_scene->addItem(m);
    m_modules.push_back(m);
    recompute();
    return m;
}

bool BenchWindow::connectSockets(BenchSocket *a, BenchSocket *b, const QColor &col)
{
    if (!a || !b || a == b) return false;
    auto *w = new BenchWire(a, b, col);
    m_scene->addItem(w);
    m_wires.push_back(w);
    recompute();
    return true;
}

void BenchWindow::removeWire(BenchWire *w)
{
    m_wires.erase(std::remove(m_wires.begin(), m_wires.end(), w), m_wires.end());
    m_scene->removeItem(w);
    delete w;
    recompute();
}

void BenchWindow::removeModule(BenchModule *m)
{
    for (BenchSocket *s : m->sockets) {
        std::vector<BenchWire *> ws = s->wires;
        for (BenchWire *w : ws) {
            m_wires.erase(std::remove(m_wires.begin(), m_wires.end(), w), m_wires.end());
            m_scene->removeItem(w);
            delete w;
        }
    }
    m_modules.erase(std::remove(m_modules.begin(), m_modules.end(), m), m_modules.end());
    m_scene->removeItem(m);
    delete m;
    recompute();
}

void BenchWindow::clearAll()
{
    while (!m_wires.empty()) { BenchWire *w = m_wires.back(); m_wires.pop_back(); m_scene->removeItem(w); delete w; }
    while (!m_modules.empty()) { BenchModule *m = m_modules.back(); m_modules.pop_back(); m_scene->removeItem(m); delete m; }
    recompute();
}

QPointF BenchWindow::findSlot(BenchModule *m) const
{
    const double yMin = m->instrument ? 40 : 510, yMax = m->instrument ? 466 : 990;
    for (double y = yMin; y + m->h <= yMax; y += 10)
        for (double x = 20; x + m->w <= 1680; x += 10) {
            const QRectF r(x, y, m->w, m->h);
            bool free = true;
            for (BenchModule *o : m_modules)
                if (o != m && o->sceneBoundingRect().adjusted(-14, -14, 14, 14).intersects(r)) { free = false; break; }
            if (free) return QPointF(x, y);
        }
    return QPointF(20 + 20 * (m_modules.size() % 10), yMin);
}

void BenchWindow::arrange()
{
    std::vector<BenchModule *> ms = m_modules;
    std::stable_sort(ms.begin(), ms.end(), [](BenchModule *a, BenchModule *b) { return a->kind < b->kind; });
    for (BenchModule *m : ms) m->setPos(-5000, -5000);
    for (BenchModule *m : ms) m->setPos(findSlot(m));
}

void BenchWindow::drawTable()
{
    auto *wood = m_scene->addRect(0, 0, 1700, 1000, Qt::NoPen, QColor(206, 172, 128));
    wood->setZValue(-20);
    auto *rail = m_scene->addRect(0, 0, 1700, 470, QPen(QColor(110, 115, 125), 3), QColor(188, 194, 204));
    rail->setZValue(-19);
    auto *edge = m_scene->addRect(0, 470, 1700, 8, Qt::NoPen, QColor(120, 90, 60));
    edge->setZValue(-18);
    auto *t1 = m_scene->addSimpleText(QStringLiteral("PUPITRE ET APPAREILS DE MESURE"));
    t1->setPos(14, 8); t1->setBrush(QColor(90, 95, 105)); t1->setZValue(-17);
    auto *t2 = m_scene->addSimpleText(QStringLiteral("PLAN DE TRAVAIL - CHARGES"));
    t2->setPos(14, 482); t2->setBrush(QColor(120, 90, 60)); t2->setZValue(-17);
}

void BenchWindow::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    if (!m_fitted) { m_fitted = true; m_view->fitInView(QRectF(0, 0, 1700, 1000), Qt::KeepAspectRatio); }
}

bool BenchWindow::recompute()
{
    // 1) reseaux = douilles reliees par des fils (union-find)
    std::vector<BenchSocket *> all;
    for (BenchModule *m : m_modules) for (BenchSocket *s : m->sockets) { s->net = (int)all.size(); all.push_back(s); }
    int N = (int)all.size();
    std::vector<int> par(N);
    for (int i = 0; i < N; ++i) par[i] = i;
    std::function<int(int)> find = [&](int x) { while (par[x] != x) { par[x] = par[par[x]]; x = par[x]; } return x; };
    for (BenchWire *w : m_wires) par[find(w->a->net)] = find(w->b->net);

    // 2) masse = neutre du pupitre (sinon premiere douille)
    int groundRoot = N ? find(0) : 0;
    for (BenchModule *m : m_modules)
        if (m->isSupply())
            for (BenchSocket *s : m->sockets) if (s->name == "N") groundRoot = find(s->net);

    std::vector<int> idOf(N, -1);
    int nets = 0;
    if (N) { idOf[groundRoot] = 0; nets = 1; }
    std::vector<int> netOf(N);
    for (int i = 0; i < N; ++i) {
        int r = find(i);
        if (idOf[r] < 0) idOf[r] = nets++;
        netOf[i] = idOf[r];
    }
    for (int i = 0; i < N; ++i) all[i]->net = netOf[i];

    // 3) elements et sources
    std::vector<BenchElem> el; std::vector<BenchSrc> src;
    for (BenchModule *m : m_modules) { m->elements(el); m->sources(src); }

    int nn = std::max(0, nets - 1), ns = (int)src.size(), D = nn + ns;
    BenchSolution sol; sol.v.assign(std::max(nets, 1), bcplx(0, 0));
    sol.all = all;
    bool ok = true;

    if (D > 0) {
        std::vector<bcplx> A((size_t)D * D, bcplx(0, 0)), b(D, bcplx(0, 0));
        auto at = [&](int r, int c) -> bcplx & { return A[(size_t)r * D + c]; };
        for (int k = 0; k < nn; ++k) at(k, k) += 1e-9;                        // evite les noeuds flottants
        for (const BenchElem &e : el) {
            int i = e.a->net - 1, j = e.b->net - 1;
            if (e.a->net == e.b->net) continue;
            if (i >= 0) at(i, i) += e.y;
            if (j >= 0) at(j, j) += e.y;
            if (i >= 0 && j >= 0) { at(i, j) -= e.y; at(j, i) -= e.y; }
        }
        for (int k = 0; k < ns; ++k) {
            int row = nn + k, ip = src[k].p->net - 1, in = src[k].n->net - 1;
            if (ip >= 0) { at(ip, row) += 1.0; at(row, ip) += 1.0; }
            if (in >= 0) { at(in, row) -= 1.0; at(row, in) -= 1.0; }
            b[row] = src[k].v;
        }
        // elimination de Gauss (pivot partiel)
        for (int c = 0; c < D && ok; ++c) {
            int piv = c;
            for (int r = c + 1; r < D; ++r) if (std::abs(at(r, c)) > std::abs(at(piv, c))) piv = r;
            if (std::abs(at(piv, c)) < 1e-12) { ok = false; break; }
            if (piv != c) { for (int k = 0; k < D; ++k) std::swap(at(c, k), at(piv, k)); std::swap(b[c], b[piv]); }
            for (int r = c + 1; r < D; ++r) {
                bcplx f = at(r, c) / at(c, c);
                if (f == bcplx(0, 0)) continue;
                for (int k = c; k < D; ++k) at(r, k) -= f * at(c, k);
                b[r] -= f * b[c];
            }
        }
        if (ok) {
            std::vector<bcplx> x(D);
            for (int r = D - 1; r >= 0; --r) {
                bcplx s = b[r];
                for (int k = r + 1; k < D; ++k) s -= at(r, k) * x[k];
                x[r] = s / at(r, r);
            }
            for (int k = 0; k < nn; ++k) sol.v[k + 1] = x[k];
        }
    }

    // 4) affichage
    for (BenchSocket *s : all) {
        bcplx v = sol.at(s);
        s->setToolTip(QStringLiteral("%1 : %2 V  %3 deg (par rapport au neutre)")
                          .arg(s->name).arg(fmt(std::abs(v), 1)).arg(fmt(std::arg(v) * 180.0 / 3.141592653589793, 0)));
    }
    QStringList msgs;
    for (BenchModule *m : m_modules) { m->warn.clear(); m->post(sol); m->update(); }
    if (!ok) msgs << QStringLiteral("Court-circuit ou circuit non resoluble : calcul impossible.");
    for (BenchModule *m : m_modules) if (!m->warn.isEmpty()) msgs << (m->title + " : " + m->warn);
    m_status = msgs.isEmpty() ? QStringLiteral("Circuit OK - 50 Hz") : msgs.join("   |   ");
    if (m_label) m_label->setText(m_status);
    return ok;
}

BenchWindow::BenchWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Voltix - Banc virtuel d'electrotechnique"));
    resize(1280, 760);
    setAttribute(Qt::WA_DeleteOnClose);

    m_scene = new BenchScene;
    m_scene->setSceneRect(-40, -40, 1780, 1080);
    m_scene->setBackgroundBrush(QColor(60, 62, 66));
    drawTable();
    m_view = new BenchView(m_scene);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::RubberBandDrag);
    setCentralWidget(m_view);

    m_label = new QLabel;
    statusBar()->addWidget(m_label, 1);

    auto *tb = addToolBar(QStringLiteral("Banc"));
    tb->setMovable(false);
    tb->addWidget(new QLabel(QStringLiteral("  Couleur du fil : ")));
    m_wireColor = new QComboBox;
    const std::pair<const char *, QColor> cols[] = {
        {"Rouge", QColor(205, 35, 35)}, {"Noir", QColor(35, 35, 38)}, {"Bleu", QColor(40, 90, 210)},
        {"Vert", QColor(40, 160, 70)}, {"Jaune", QColor(230, 200, 20)}, {"Marron", QColor(125, 75, 40)},
        {"Gris", QColor(140, 140, 145)}};
    for (auto &c : cols) m_wireColor->addItem(QString::fromUtf8(c.first), c.second);
    tb->addWidget(m_wireColor);

    QMenu *add = menuBar()->addMenu(QStringLiteral("&Ajouter un module"));
    for (const QString &k : kinds()) add->addAction(k, this, [this, k] { addModule(k); });
    QMenu *edit = menuBar()->addMenu(QStringLiteral("&Banc"));
    edit->addAction(QStringLiteral("Supprimer la selection (Suppr)"), this, [this] { m_scene->onDeleteSelection(); });
    edit->addAction(QStringLiteral("Ranger les modules sur la table"), this, [this] { arrange(); });
    edit->addAction(QStringLiteral("Nouveau banc (tout effacer)"), this, [this] { clearAll(); });
    edit->addSeparator();
    QAction *zin = edit->addAction(QStringLiteral("Zoom +"));
    zin->setShortcut(QKeySequence::ZoomIn);
    connect(zin, &QAction::triggered, this, [this] { m_view->scale(1.2, 1.2); });
    QAction *zout = edit->addAction(QStringLiteral("Zoom -"));
    zout->setShortcut(QKeySequence::ZoomOut);
    connect(zout, &QAction::triggered, this, [this] { m_view->scale(1 / 1.2, 1 / 1.2); });
    menuBar()->addMenu(QStringLiteral("&Aide"))->addAction(QStringLiteral("Mode d'emploi"), this, [this] {
        m_label->setText(QStringLiteral("Glisser d'une douille a une autre = fil. Clic droit sur un fil = le retirer. "
                                        "Double-clic sur un module = reglages. Molette = reglage principal."));
    });

    m_scene->onConnect = [this](BenchSocket *a, BenchSocket *b) {
        connectSockets(a, b, m_wireColor->currentData().value<QColor>());
    };
    m_scene->onDeleteWire = [this](BenchWire *w) { removeWire(w); };
    m_scene->onDeleteSelection = [this] {
        std::vector<BenchWire *> ws; std::vector<BenchModule *> ms;
        for (QGraphicsItem *it : m_scene->selectedItems()) {
            if (auto *w = dynamic_cast<BenchWire *>(it)) ws.push_back(w);
            else if (auto *m = dynamic_cast<BenchModule *>(it)) ms.push_back(m);
        }
        for (BenchModule *m : ms) removeModule(m);
        for (BenchWire *w : ws)
            if (std::find(m_wires.begin(), m_wires.end(), w) != m_wires.end()) removeWire(w);
    };

    // banc de depart : pupitre + rheostat + pince
    addModule("Pupitre d'alimentation");
    addModule("Pince multifonction");
    addModule("Oscilloscope 2 voies");
    addModule("Sonde differentielle");
    addModule("Rheostat 1400 ohm / 1 A");
    recompute();
}
