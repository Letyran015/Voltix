#include "bench.h"
#include <memory>
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
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
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
    { std::vector<BenchModule *> cs = clamps; clamps.clear(); for (BenchModule *m : cs) m->onWireRemoved(this); }
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

QPainterPath BenchWire::shape() const
{
    QPainterPathStroker st; st.setWidth(14);
    return st.createStroke(path());
}

QRectF BenchWire::boundingRect() const { return QGraphicsPathItem::boundingRect().adjusted(-22, -22, 22, 22); }

void BenchWire::paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w)
{
    QGraphicsPathItem::paint(p, o, w);
    if (clamped) {
        const QPointF m = path().pointAtPercent(0.5);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor(25, 25, 28), 9));
        p->drawEllipse(m, 11, 11);
        p->setPen(QPen(QColor(60, 190, 90), 4));
        p->drawEllipse(m, 11, 11);
    }
}

BenchJaw::BenchJaw(BenchModule *m, QPointF pos, double r) : QGraphicsEllipseItem(-r, -r, 2 * r, 2 * r, m), module(m)
{
    setPos(pos);
    setZValue(2);
    setToolTip(QStringLiteral("Anneau de la pince : cliquez dessus, puis cliquez sur un fil pour l'enserrer. Re-cliquez pour la retirer."));
}

void BenchJaw::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    const double r = rect().width() / 2;
    const bool on = module->clampedWire() != nullptr;
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(70, 72, 78), 3));
    p->drawEllipse(QPointF(0, 0), r, r);
    if (on) { p->setPen(QPen(QColor(60, 190, 90), 4)); p->drawEllipse(QPointF(0, 0), r - 5, r - 5); }
    else { p->setPen(QPen(QColor(200, 60, 60), 2, Qt::DashLine)); p->drawEllipse(QPointF(0, 0), r - 5, r - 5); }
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
    {
        const QPainterPath bp = bodyPath();
        if (bp.isEmpty()) p->drawRoundedRect(QRectF(1, 1, w - 2, h - 2), 8, 8);
        else p->drawPath(bp);
    }

    QColor txt = body.lightness() < 120 ? QColor(240, 240, 240) : QColor(25, 25, 25);
    p->setPen(txt);
    QFont f = p->font(); f.setPointSize(9); f.setBold(true); p->setFont(f);
    p->drawText(QRectF(6, titleY(), w - 12, 18), Qt::AlignCenter, title);

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
        if (pendingClamp) {
            BenchModule *pc = pendingClamp;
            pendingClamp = nullptr;
            for (QGraphicsItem *it : items(e->scenePos()))
                if (auto *w = dynamic_cast<BenchWire *>(it)) {
                    pc->clampTo(w);
                    if (onHint) onHint(QStringLiteral("Pince posee sur le fil."));
                    if (onClampChanged) onClampChanged();
                    e->accept();
                    return;
                }
            if (onHint) onHint(QStringLiteral("Pose de la pince annulee."));
        }
        for (QGraphicsItem *it : items(e->scenePos()))
            if (auto *j = dynamic_cast<BenchJaw *>(it)) {
                BenchModule *m = j->module;
                if (m->clampedWire()) { m->clampTo(nullptr); if (onClampChanged) onClampChanged(); }
                else { pendingClamp = m; if (onHint) onHint(QStringLiteral("Cliquez maintenant sur le fil a enserrer.")); }
                j->update();
                e->accept();
                return;
            }
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
    void elements(std::vector<BenchElem> &e) override { e.push_back({a, b, 1.0 / z(), 1.0 / std::max(params[1].value, 1e-3)}); }
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
            e.push_back({s[k][0], s[k][1], bcplx(0, PI2 * BenchWindow::kFreq * params[k].value * 1e-6), 0.0});
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
        so->scale = [this] { return 1.0 / params[0].value; };
        so->unit = "A";
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
        out->sourceDc = [this](const BenchSolution &s) { return (s.dcAt(p) - s.dcAt(m)) / params[0].value; };
        out->scale = [this] { return params[0].value; };
        out->unit = "V";
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
        params = { {"CH1 : sensibilite par division (V ou A selon la sonde)", "", 0.001, 1000, 100, 10, 3},
                   {"CH2 : sensibilite par division (V ou A selon la sonde)", "", 0.001, 1000, 0.5, 0.1, 3},
                   {"Base de temps", "ms/div", 0.1, 100, 5, 1, 1} };
        wheelParam = 2;
    }
    bool on1() const { return !c1->wires.empty(); }
    bool on2() const { return !c2->wires.empty(); }
    void read(const BenchSolution &s, BenchSocket *c, bcplx &ac, double &dc) const {
        ac = s.at(c) - (gnd->wires.empty() ? bcplx(0, 0) : s.at(gnd));
        dc = s.dcAt(c) - (gnd->wires.empty() ? 0.0 : s.dcAt(gnd));
        for (BenchSocket *t : s.all)
            if (t != c && t->net == c->net) {
                if (t->source)   ac += t->source(s);
                if (t->sourceDc) dc += t->sourceDc(s);
            }
    }
    // sonde branchee sur la voie : rapport (grandeur reelle / volt) et unite affichee
    void probeOf(const BenchSolution &s, const BenchSocket *c, double &k, QString &u) const {
        k = 1.0; u = QStringLiteral("V");
        for (BenchSocket *t : s.all)
            if (t != c && t->net == c->net && t->source && t->scale) { k = t->scale(); u = t->unit; return; }
    }
    void post(const BenchSolution &s) override {
        read(s, c1, v1, d1); read(s, c2, v2, d2);
        probeOf(s, c1, k1, u1); probeOf(s, c2, k2, u2);
    }
    static QPainterPath trace(const bcplx &v, double dc, double vdiv, double msdiv, double t0) {
        QPainterPath path;
        const double w = PI2 * BenchWindow::kFreq;
        for (int px = 0; px <= 400; px += 2) {
            double t = msdiv * 1e-3 * 10.0 * px / 400.0 + t0;
            double val = std::real(std::sqrt(2.0) * v * std::polar(1.0, w * t)) + dc;
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
        bool a1 = on1() && std::abs(v1) > 1e-9, a2 = on2() && std::abs(v2) > 1e-9;   // phase : composante alternative
        double ph = a1 ? std::arg(v1) : (a2 ? std::arg(v2) : 0.0);
        double t0 = (-PI2 / 4 - ph) / w;                 // declenchement : front montant de la voie 1
        p->save();
        p->setClipRect(scr);
        p->setRenderHint(QPainter::Antialiasing);
        p->setBrush(Qt::NoBrush);                        // sinon le trace est rempli et masque l'autre voie
        if (on1()) { p->setPen(QPen(QColor(235, 205, 60), 2)); p->drawPath(trace(v1, d1, params[0].value / k1, params[2].value, t0)); }
        if (on2()) { p->setPen(QPen(QColor(60, 200, 220), 2)); p->drawPath(trace(v2, d2, params[1].value / k2, params[2].value, t0)); }
        p->restore();

        QFont f = p->font(); f.setPointSize(8); f.setBold(false); p->setFont(f);
        p->setPen(QColor(235, 205, 60));
        p->drawText(QPointF(24, 361), QStringLiteral("CH1 %1 %3/div  eff %2 %3").arg(QString::number(params[0].value, 'g', 4)).arg(QString::number(std::abs(v1) * k1, 'g', 4)).arg(u1));
        p->setPen(QColor(60, 200, 220));
        p->drawText(QPointF(24, 375), QStringLiteral("CH2 %1 %3/div  eff %2 %3").arg(QString::number(params[1].value, 'g', 4)).arg(QString::number(std::abs(v2) * k2, 'g', 4)).arg(u2));
        p->setPen(QColor(230, 230, 230));
        QString dphi = (a1 && a2) ? fmt(std::arg(v2 / v1) * 180.0 / 3.141592653589793, 1) + QStringLiteral(" deg") : QStringLiteral("-");
        p->drawText(QPointF(250, 361), QStringLiteral("%1 ms/div").arg(fmt(params[2].value, 1)));
        p->drawText(QPointF(250, 375), QStringLiteral("Dephasage CH2/CH1 : ") + dphi);
    }
    BenchSocket *c1, *c2, *gnd;
    bcplx v1{0, 0}, v2{0, 0};
    double d1 = 0, d2 = 0, k1 = 1, k2 = 1;
    QString u1 = "V", u2 = "V";
};

// ---- Multimetre numerique (V alt./cont., A alt./cont.) -----------------------------------------------
class Multimeter : public BenchModule {
public:
    Multimeter() : BenchModule(QStringLiteral("Multimetre numerique"), 250, 215, QColor(225, 190, 50)) {
        instrument = true;
        v   = addSocket("V", cRed, 50, 180);
        com = addSocket("COM", cBlack, 125, 180);
        a   = addSocket("A", cRed, 200, 180);
        params = { {"Fonction (0 = V alt., 1 = V cont., 2 = A alt., 3 = A cont.)", "", 0, 3, 0, 1, 0} };
        wheelParam = 0;
    }
    int mode() const { return int(params[0].value + 0.5); }
    void elements(std::vector<BenchElem> &e) override {
        if (mode() >= 2) e.push_back({a, com, 1.0 / kShunt});                 // ampermetre : quasi court-circuit
    }
    void post(const BenchSolution &s) override {
        switch (mode()) {
        case 0: value = std::abs(s.at(v) - s.at(com)); break;
        case 1: value = s.dcAt(v) - s.dcAt(com); break;
        case 2: value = std::abs((s.at(a) - s.at(com)) / kShunt); break;
        default: value = (s.dcAt(a) - s.dcAt(com)) / kShunt; break;
        }
        if (mode() >= 2 && std::abs(value) > 10.0) warn = QStringLiteral("CALIBRE 10 A DEPASSE");
    }
    void paintExtra(QPainter *p) override {
        p->setPen(QPen(QColor(10, 10, 10), 2));
        p->setBrush(QColor(160, 185, 150));
        p->drawRoundedRect(QRectF(20, 32, 210, 75), 5, 5);
        const char *unit[4] = {"V ~", "V =", "A ~", "A ="};
        QFont f("Monospace"); f.setStyleHint(QFont::TypeWriter); f.setPointSize(20); f.setBold(true); p->setFont(f);
        p->setPen(QColor(20, 30, 15));
        p->drawText(QRectF(25, 38, 200, 45), Qt::AlignRight | Qt::AlignVCenter, fmt(value, std::abs(value) < 10 ? 3 : 2));
        f.setPointSize(10); p->setFont(f);
        p->drawText(QRectF(25, 82, 200, 20), Qt::AlignRight | Qt::AlignVCenter, QString::fromLatin1(unit[mode()]));
        f.setPointSize(8); f.setBold(false); p->setFont(f);
        p->setPen(QColor(40, 40, 40));
        p->drawText(QRectF(8, 118, 234, 40), Qt::AlignCenter | Qt::TextWordWrap,
                    mode() < 2 ? QStringLiteral("Tension : bornes V et COM en parallele")
                               : QStringLiteral("Courant : bornes A et COM EN SERIE dans le circuit"));
    }
    static constexpr double kShunt = 1e-3;
    BenchSocket *v, *com, *a;
    double value = 0;
};

// ---- Alimentation continue double -------------------------------------------------------------------------
class DcSupply : public BenchModule {
public:
    DcSupply() : BenchModule(QStringLiteral("Alimentation continue 2 x 30 V"), 330, 190, QColor(80, 86, 96)) {
        instrument = true;
        p1 = addSocket("1+", cRed, 50, 150);
        m1 = addSocket("1-", cBlack, 105, 150);
        p2 = addSocket("2+", cRed, 225, 150);
        m2 = addSocket("2-", cBlack, 280, 150);
        params = { {"Tension voie 1", "V", 0, 30, 10, 0.5, 2},
                   {"Tension voie 2", "V", 0, 30, 10, 0.5, 2},
                   {"Sortie (0 = coupee, 1 = active)", "", 0, 1, 1, 1, 0} };
        wheelParam = 0;
    }
    bool on() const { return params[2].value > 0.5; }
    bool isSupply() const override { return true; }
    void sources(std::vector<BenchSrc> &s) override {
        if (!on()) return;
        s.push_back({p1, m1, bcplx(0, 0), params[0].value});
        s.push_back({p2, m2, bcplx(0, 0), params[1].value});
    }
    void paintExtra(QPainter *g) override {
        g->setPen(QPen(QColor(10, 10, 10), 2));
        g->setBrush(QColor(20, 30, 20));
        g->drawRoundedRect(QRectF(20, 32, 130, 60), 4, 4);
        g->drawRoundedRect(QRectF(180, 32, 130, 60), 4, 4);
        QFont f("Monospace"); f.setStyleHint(QFont::TypeWriter); f.setPointSize(16); f.setBold(true); g->setFont(f);
        g->setPen(on() ? QColor(90, 230, 110) : QColor(70, 90, 70));
        g->drawText(QRectF(20, 32, 130, 60), Qt::AlignCenter, fmt(on() ? params[0].value : 0, 2) + " V");
        g->drawText(QRectF(180, 32, 130, 60), Qt::AlignCenter, fmt(on() ? params[1].value : 0, 2) + " V");
    }
    BenchSocket *p1, *m1, *p2, *m2;
};

// ---- Bornier de distribution : deux rangees de 4 douilles reliees entre elles -------------------------
class Bornier : public BenchModule {
public:
    Bornier() : BenchModule(QStringLiteral("Bornier (2 x 4 douilles)"), 260, 150, QColor(200, 204, 196)) {
        for (int k = 0; k < 4; ++k) {
            top[k] = addSocket(QStringLiteral("A%1").arg(k + 1), cBlack, 40 + 60 * k, 60);
            bot[k] = addSocket(QStringLiteral("B%1").arg(k + 1), cBlack, 40 + 60 * k, 115);
        }
    }
    bool hasLinks() const override { return true; }
    void links(std::vector<std::pair<BenchSocket *, BenchSocket *>> &l) override {
        for (int k = 0; k < 3; ++k) { l.push_back({top[k], top[k + 1]}); l.push_back({bot[k], bot[k + 1]}); }
    }
    void paintExtra(QPainter *g) override {
        g->setPen(QPen(QColor(120, 120, 120), 6, Qt::SolidLine, Qt::RoundCap));
        g->drawLine(QPointF(40, 60), QPointF(220, 60));
        g->drawLine(QPointF(40, 115), QPointF(220, 115));
    }
    BenchSocket *top[4], *bot[4];
};

// ---- Courant dans un fil (pour les pinces a machoire) ------------------------------------------------------
// Courant qui traverse le fil w, compte de a vers b (puis inverse si b est le pupitre : sens source -> charge).
// Il est deduit de la douille d'une extremite qui n'a qu'un seul fil et appartient a un appareil sans liaison interne.
bool wireCurrent(const BenchSolution &s, const BenchWire *w, bcplx &I, double &Idc)
{
    BenchSocket *ends[2] = {w->a, w->b};
    for (int k = 0; k < 2; ++k) {
        BenchSocket *e = ends[k];
        if (e->wires.size() != 1 || e->module->hasLinks()) continue;
        const bcplx in = s.iinAt(e);
        const double ind = s.iinDcAt(e);
        I   = (k == 0) ? -in  : in;
        Idc = (k == 0) ? -ind : ind;
        if (w->b->module->isSupply() && !w->a->module->isSupply()) { I = -I; Idc = -Idc; }
        return true;
    }
    return false;
}

// ---- Base des pinces : anneau cliquable + fil pince -----------------------------------------------------------
class ClampBase : public BenchModule {
public:
    ClampBase(const QString &t, int w_, int h_, const QColor &c) : BenchModule(t, w_, h_, c) {
        instrument = true;
        w = 250;
        jaw = new BenchJaw(this, QPointF(w_ / 2.0, 84), 38);
    }
    ~ClampBase() override {
        detach();
    }
    void detach() {
        if (!wire) return;
        auto &v = wire->clamps;
        v.erase(std::remove(v.begin(), v.end(), (BenchModule *)this), v.end());
        wire->refreshClamp();
        wire = nullptr;
    }
    bool canClamp() const override { return true; }
    BenchWire *clampedWire() const override { return wire; }
    void clampTo(BenchWire *w) override {
        detach();
        wire = w;
        if (w) { w->clamps.push_back(this); w->refreshClamp(); }
        update();
        if (jaw) jaw->update();
    }
    void onWireRemoved(BenchWire *w) override { if (wire == w) { wire = nullptr; if (jaw) jaw->update(); } }
    // 0 = pas de fil, 1 = ok, 2 = fil ambigu ; I et Idc selon le sens choisi
    int current(const BenchSolution &s, bcplx &I, double &Idc) const {
        if (!wire) return 0;
        if (!wireCurrent(s, wire, I, Idc)) return 2;
        if (flip() > 0.5) { I = -I; Idc = -Idc; }
        return 1;
    }
    virtual double flip() const { return 0; }
    // ---- dessin commun des pinces : machoire rouge effilee, collerette, corps sombre, gachette ----
    static QPainterPath holePath() {
        QPainterPath h;
        h.moveTo(98, 138); h.lineTo(152, 138);
        h.cubicTo(170, 108, 184, 70, 168, 42);
        h.cubicTo(155, 20, 95, 20, 82, 42);
        h.cubicTo(66, 70, 80, 108, 98, 138);
        return h;
    }
    static QPainterPath jawOuter() {
        QPainterPath j;
        j.moveTo(58, 152); j.lineTo(36, 72); j.quadTo(30, 4, 100, 3);
        j.lineTo(150, 3); j.quadTo(220, 4, 214, 72); j.lineTo(192, 152); j.closeSubpath();
        return j;
    }
    static QPainterPath triggerPath() {
        QPainterPath t;
        t.moveTo(30, 205); t.lineTo(5, 238); t.quadTo(0, 262, 18, 267); t.lineTo(34, 267); t.closeSubpath();
        return t;
    }
    QPainterPath bodyPath() const override {
        QPainterPath b; b.addRoundedRect(QRectF(bx0, 150, bx1 - bx0, h - 151), 16, 16);
        QPainterPath col; col.addRoundedRect(QRectF(8, 148, 234, 20), 10, 10);
        return b.united(col).united(jawOuter()).united(triggerPath()).subtracted(holePath());
    }
    double titleY() const override { return 149; }
    void paintDeco(QPainter *p, const QString &cat) {
        p->setBrush(QColor(196, 32, 38));
        p->setPen(QPen(QColor(120, 18, 22), 1.5));
        p->drawPath(jawOuter().united(triggerPath()).subtracted(holePath()));
        p->setPen(QPen(QColor(120, 18, 22), 2));
        p->drawLine(QPointF(125, 3), QPointF(125, 21));                              // fente du haut de la machoire
        QPolygonF plate; plate << QPointF(52, 112) << QPointF(198, 112) << QPointF(194, 148) << QPointF(56, 148);
        p->setBrush(QColor(48, 48, 52)); p->setPen(QPen(QColor(25, 25, 28), 1.5));
        p->drawPolygon(plate);
        p->setPen(QColor(235, 235, 235));
        QFont f = p->font(); f.setPointSize(8); f.setBold(true); p->setFont(f);
        p->drawText(QRectF(52, 120, 146, 22), Qt::AlignCenter, cat);
        p->setPen(QPen(QColor(105, 105, 112), 2)); p->setBrush(Qt::NoBrush);          // arete de la collerette
        p->drawRoundedRect(QRectF(8, 148, 234, 20), 10, 10);
    }
    double bx0 = 22, bx1 = 228;
    BenchWire *wire = nullptr;
    BenchJaw *jaw = nullptr;
};

// ---- Pince wattmetre (types F205, F407, F27) --------------------------------------------------------------------
class PowerClamp : public ClampBase {
public:
    PowerClamp(int model)
        : ClampBase(model == 0 ? QStringLiteral("Pince wattmetre 600 A")
                    : model == 1 ? QStringLiteral("Pince wattmetre 1000 A")
                                 : QStringLiteral("Pince wattmetre 1000 A (ancien)"),
                    250, model == 2 ? 612 : 565, QColor(58, 58, 62)), model_(model) {
        com = addSocket("COM", QColor(20, 20, 22), 80, model == 2 ? 575 : 525);
        vp  = addSocket("V+", cRed, 170, model == 2 ? 575 : 525);
        if (model == 2) { bx0 = 14; bx1 = 236; }
        params = { {"Fonction (0 OFF, 1 V, 2 A, 3 W, 4 ohm)", "", 0, 4, 3, 1, 0},
                   {"Grandeur en W (0 P, 1 S, 2 Q, 3 PF)", "", 0, 3, 0, 1, 0},
                   {"Maintien de l'affichage (HOLD)", "", 0, 1, 0, 1, 0},
                   {"Sens de la pince (0 normal, 1 inverse)", "", 0, 1, 0, 1, 0},
                   {"Afficher la frequence (Hz)", "", 0, 1, 0, 1, 0} };
        wheelParam = 0;
    }
    double flip() const override { return params[3].value; }
    int fn() const { return int(params[0].value + 0.5); }
    static QString num(double x) {
        const double a = std::abs(x);
        return fmt(x, a < 10 ? 3 : (a < 100 ? 2 : (a < 1000 ? 1 : 0)));
    }
    void post(const BenchSolution &s) override {
        const bcplx uac = s.at(vp) - s.at(com);
        const double udc = s.dcAt(vp) - s.dcAt(com);
        bcplx iac(0, 0); double idc = 0;
        const int st = current(s, iac, idc);
        const double urms = std::hypot(std::abs(uac), udc), irms = std::hypot(std::abs(iac), idc);
        const bcplx sp = uac * std::conj(iac);
        const double P = sp.real() + udc * idc, Q = sp.imag(), S = urms * irms, PF = S > 1e-9 ? P / S : 0;
        main_ = "---"; unit_ = ""; sub_.clear();
        const bool needI = (fn() == 2 || fn() == 3);
        if (fn() == 0) main_.clear();
        else if (fn() == 1) { main_ = num(urms); unit_ = "V"; sub_ = "TRMS AC+DC"; }
        else if (fn() == 4 && model_ == 2) { main_ = "PrGM"; unit_ = ""; sub_ = QStringLiteral("non disponible"); }
        else if (fn() == 4) {
            if (std::abs(uac) > 5 || std::abs(udc) > 5) { main_ = "OL"; unit_ = "ohm"; sub_ = QStringLiteral("circuit sous tension !"); warn = QStringLiteral("OHMMETRE SOUS TENSION"); }
            else {
                const double r = s.rdc ? s.rdc(vp, com) : 1e12;
                if (r > 5e7) { main_ = "OL"; unit_ = "ohm"; sub_ = QStringLiteral("circuit ouvert"); }
                else if (r >= 1000) { main_ = num(r / 1000.0); unit_ = "kohm"; }
                else { main_ = num(r); unit_ = "ohm"; }
            }
        }
        else if (st == 0) sub_ = QStringLiteral("Cliquez sur l'anneau, puis sur un fil");
        else if (st == 2) { main_ = "?"; warn = QStringLiteral("FIL AMBIGU"); sub_ = QStringLiteral("plusieurs fils sur la douille"); }
        else if (fn() == 2) { main_ = num(irms); unit_ = "A"; sub_ = "TRMS AC+DC"; }
        else {
            const int g = int(params[1].value + 0.5);
            if (g == 0) { main_ = num(P); unit_ = "W"; }
            else if (g == 1) { main_ = num(S); unit_ = "VA"; }
            else if (g == 2) { main_ = num(Q); unit_ = "var"; }
            else { main_ = fmt(PF, 2); unit_ = "PF"; }
            sub_ = QStringLiteral("%1 V  %2 A").arg(num(urms), num(irms));
            if (std::abs(Q) > 0.05 * S && S > 1e-6) sub_ += Q > 0 ? "  ind" : "  cap";
        }
        (void)needI;
        if (params[4].value > 0.5) sub_ = QStringLiteral("%1 Hz").arg((std::abs(uac) > 0.5 || std::abs(iac) > 1e-3) ? "50.0" : "0.0");
        const bool hold = params[2].value > 0.5;
        if (hold && !wasHold_) { hMain_ = main_; hUnit_ = unit_; hSub_ = sub_; }
        wasHold_ = hold;
        if (hold) { main_ = hMain_; unit_ = hUnit_; sub_ = hSub_; }
        const double vmax = model_ == 1 ? 1000 : 600;
        if (urms > vmax) warn = QStringLiteral("TENSION > %1 V").arg(vmax);
    }
    void lcd(QPainter *p, const QRectF &r, int big) {
        p->setPen(QPen(QColor(15, 15, 15), 2)); p->setBrush(QColor(165, 188, 155));
        p->drawRoundedRect(r, 5, 5);
        p->setPen(QColor(20, 30, 15));
        QFont m("Monospace"); m.setStyleHint(QFont::TypeWriter); m.setBold(true); m.setPointSize(big); p->setFont(m);
        p->drawText(QRectF(r.x() + 4, r.y() + 4, r.width() - 8, r.height() * 0.45), Qt::AlignRight | Qt::AlignVCenter, main_);
        m.setPointSize(10); p->setFont(m);
        p->drawText(QRectF(r.x() + 4, r.y() + r.height() * 0.46, r.width() - 8, 18), Qt::AlignRight | Qt::AlignVCenter, unit_);
        m.setPointSize(7); m.setBold(false); p->setFont(m);
        p->drawText(QRectF(r.x() + 5, r.y() + r.height() * 0.66, r.width() - 10, r.height() * 0.32), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, sub_);
        if (params[2].value > 0.5) { m.setBold(true); p->setFont(m); p->drawText(QRectF(r.x() + 5, r.y() + 3, 50, 12), Qt::AlignLeft, "HOLD"); }
    }
    void knob(QPainter *p, QPointF c, const QPointF &dir, double rr) {
        p->setPen(QPen(QColor(25, 25, 28), 1.5)); p->setBrush(QColor(112, 112, 118));
        p->drawEllipse(c, rr, rr);
        p->setPen(QPen(QColor(80, 80, 86), 2));
        for (int k = 0; k < 28; ++k) {                                   // molettage du bord
            const double a = k * 6.2831853 / 28;
            p->drawLine(c + QPointF(std::cos(a), std::sin(a)) * (rr - 1), c + QPointF(std::cos(a), std::sin(a)) * (rr - 5));
        }
        p->setPen(QPen(QColor(25, 25, 28), 1.5)); p->setBrush(QColor(46, 46, 50));
        p->drawEllipse(c, rr - 8, rr - 8);
        p->setPen(QPen(QColor(235, 235, 235), 5, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(c, c + dir);
    }
    void key(QPainter *p, QRectF r, const QColor &col, const QString &t, const QColor &tc = QColor(15, 15, 15)) {
        p->setPen(QPen(QColor(10, 10, 10), 1.5)); p->setBrush(col);
        p->drawRoundedRect(r, 6, 6);
        QFont k = p->font(); k.setPointSize(6); k.setBold(true); p->setFont(k);
        p->setPen(tc); p->drawText(r, Qt::AlignCenter, t);
    }
    void paintExtra(QPainter *p) override {
        paintDeco(p, model_ == 1 ? QStringLiteral("1000 V CAT IV   1000 A") : (model_ == 0 ? QStringLiteral("600 V CAT IV   600 A") : QStringLiteral("600 V CAT III   1000 A")));
        QFont f = p->font(); f.setBold(true); f.setPointSize(8); p->setFont(f);
        const QColor white(236, 236, 236), yellow(232, 200, 40);
        if (model_ != 2) {
            const QPointF dc(150, 232);
            struct L { const char *t; QPointF v; QColor c; };
            const L labs[5] = { {"OFF", QPointF(0, 58), white}, {"V", QPointF(-56, 42), white}, {"A", QPointF(-66, -10), white},
                                {"W", QPointF(-52, -40), white}, {"ohm", QPointF(-66, 16), yellow} };
            for (int k = 0; k < 5; ++k) { p->setPen(labs[k].c); p->drawText(QRectF(dc.x() + labs[k].v.x() - 24, dc.y() + labs[k].v.y() - 8, 48, 16), Qt::AlignCenter, labs[k].t); }
            p->setPen(white); f.setPointSize(6); p->setFont(f);
            p->drawText(QRectF(36, 178, 70, 12), Qt::AlignLeft, "VA var PF");
            const int fi = std::max(0, std::min(4, fn()));
            QPointF d = labs[fi].v; if (fi == 0) d = QPointF(0, 40);
            const double len = std::hypot(d.x(), d.y());
            knob(p, dc, d / len * 28.0, 42);
            // touches
            key(p, QRectF(42, 322, 48, 24), QColor(70, 130, 215), "HOLD", QColor(255, 255, 255));
            key(p, QRectF(104, 322, 48, 24), yellow, QStringLiteral("1F/3F"));
            key(p, QRectF(166, 322, 48, 24), QColor(46, 46, 50), model_ == 1 ? "^" : "*", white);
            key(p, QRectF(42, 356, 48, 24), QColor(46, 46, 50), "MAX/MIN", white);
            key(p, QRectF(104, 356, 48, 24), QColor(190, 190, 195), "Hz");
            key(p, QRectF(166, 356, 48, 24), QColor(46, 46, 50), model_ == 1 ? "v" : "REL", white);
            lcd(p, QRectF(44, 394, 162, 106), 21);
            p->setPen(QPen(QColor(200, 30, 35), 4)); p->setBrush(Qt::NoBrush);        // anneau rouge de la borne V+
            p->drawEllipse(QPointF(170, 525), 17, 17);
        } else {
            const QPointF dc(135, 244);
            struct L { const char *t; QPointF v; };
            const L labs[5] = { {"OFF", QPointF(0, -56)}, {"V", QPointF(-62, -38)}, {"A", QPointF(-72, -8)}, {"W", QPointF(-70, 24)}, {"PRGM", QPointF(-30, 56)} };
            p->setPen(white);
            for (int k = 0; k < 4; ++k) p->drawText(QRectF(dc.x() + labs[k].v.x() - 24, dc.y() + labs[k].v.y() - 8, 48, 16), Qt::AlignCenter, labs[k].t);
            p->drawText(QRectF(60, 292, 150, 14), Qt::AlignCenter, "PRGM");
            key(p, QRectF(172, 178, 44, 18), QColor(110, 120, 190), "HOLD", white);
            const int fi = std::max(0, std::min(4, fn()));
            QPointF d = labs[fi].v; if (fi == 0) d = QPointF(0, -40); if (fi == 4) d = QPointF(-24, 40);
            const double len = std::hypot(d.x(), d.y());
            knob(p, dc, d / len * 28.0, 42);
            p->setPen(Qt::NoPen); p->setBrush(QColor(168, 174, 180));                 // boitier translucide de l'afficheur
            p->drawRoundedRect(QRectF(30, 322, 190, 168), 8, 8);
            lcd(p, QRectF(44, 336, 162, 140), 22);
            p->setBrush(QColor(28, 28, 30));                                          // clavier
            p->drawRoundedRect(QRectF(30, 498, 190, 56), 8, 8);
            const char *kl[8] = {"MIN", "SMOOTH", "HARM", "^", "AC/DC", "Hz", "v", ""};
            for (int k = 0; k < 8; ++k) {
                if (!kl[k][0]) continue;
                key(p, QRectF(38 + (k % 4) * 44, 506 + (k / 4) * 24, 38, 18), QColor(58, 58, 62), kl[k], white);
            }
        }
    }
    int model_;
    BenchSocket *com, *vp;
    QString main_, unit_, sub_, hMain_, hUnit_, hSub_;
    bool wasHold_ = false;
};

// ---- Pince de courant (sortie vers l'oscilloscope) ---------------------------------------------------------------
class CurrentProbe : public ClampBase {
public:
    CurrentProbe() : ClampBase(QStringLiteral("Pince de courant (sortie oscillo)"), 250, 330, QColor(58, 58, 62)) {
        out = addSocket("Sortie", QColor(150, 150, 155), 125, 285);
        params = { {"Sensibilite", "V/A", 0.001, 10, 0.1, 0.05, 3},
                   {"Sens de la pince (0 normal, 1 inverse)", "", 0, 1, 0, 1, 0} };
        out->scale = [this] { return 1.0 / params[0].value; };
        out->unit = "A";
        out->source = [this](const BenchSolution &s) {
            bcplx I(0, 0); double d = 0;
            return current(s, I, d) == 1 ? I * params[0].value : bcplx(0, 0);
        };
        out->sourceDc = [this](const BenchSolution &s) {
            bcplx I(0, 0); double d = 0;
            return current(s, I, d) == 1 ? d * params[0].value : 0.0;
        };
    }
    double flip() const override { return params[1].value; }
    void paintExtra(QPainter *p) override {
        paintDeco(p, QStringLiteral("CAT III   100 A"));
        p->setPen(QColor(236, 236, 236));
        QFont f = p->font(); f.setPointSize(9); f.setBold(true); p->setFont(f);
        p->drawText(QRectF(30, 190, 190, 18), Qt::AlignCenter, QStringLiteral("%1 V/A").arg(fmt(params[0].value, 3)));
        f.setBold(false); f.setPointSize(7); p->setFont(f);
        p->drawText(QRectF(30, 214, 190, 40), Qt::AlignCenter | Qt::TextWordWrap,
                    wire ? QStringLiteral("Pince posee sur un fil") : QStringLiteral("Cliquez sur l'anneau, puis sur un fil"));
    }
    BenchSocket *out;
};

} // namespace

// =====================================================================================
//  Fenetre
// =====================================================================================
QStringList BenchWindow::kinds()
{
    return { "Pupitre d'alimentation", "Rheostat 1400 ohm / 1 A", "Rheostat 2000 ohm / 0,2 A",
             "Boite a decades R50", "Charge inductive", "Condensateurs 3 x 5 uF",
             "Lampe a incandescence", "Pince multifonction (serie)", "Oscilloscope 2 voies", "Sonde differentielle",
             "Multimetre numerique", "Alimentation continue 2 x 30 V", "Bornier de distribution",
             "Pince wattmetre 600 A (F205)", "Pince wattmetre 1000 A (F407)", "Pince wattmetre 1000 A (F27)",
             "Pince de courant (sortie oscilloscope)" };
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
    case 10: m = new Multimeter; break;
    case 11: m = new DcSupply; break;
    case 12: m = new Bornier; break;
    case 13: m = new PowerClamp(0); break;
    case 14: m = new PowerClamp(1); break;
    case 15: m = new PowerClamp(2); break;
    case 16: m = new CurrentProbe; break;
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
    const double yMin = m->instrument ? 40 : 700, yMax = m->instrument ? 656 : 1240;
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
    auto *wood = m_scene->addRect(0, 0, 1700, 1250, Qt::NoPen, QColor(206, 172, 128));
    wood->setZValue(-20);
    auto *rail = m_scene->addRect(0, 0, 1700, 660, QPen(QColor(110, 115, 125), 3), QColor(188, 194, 204));
    rail->setZValue(-19);
    auto *edge = m_scene->addRect(0, 660, 1700, 8, Qt::NoPen, QColor(120, 90, 60));
    edge->setZValue(-18);
    auto *t1 = m_scene->addSimpleText(QStringLiteral("PUPITRE ET APPAREILS DE MESURE"));
    t1->setPos(14, 8); t1->setBrush(QColor(90, 95, 105)); t1->setZValue(-17);
    auto *t2 = m_scene->addSimpleText(QStringLiteral("PLAN DE TRAVAIL - CHARGES"));
    t2->setPos(14, 672); t2->setBrush(QColor(120, 90, 60)); t2->setZValue(-17);
}

bool BenchWindow::saveToFile(const QString &path) const
{
    QJsonArray ms, ws;
    for (BenchModule *m : m_modules) {
        QJsonArray pv;
        for (const BenchParam &p : m->params) pv.append(p.value);
        const int wi = m->clampedWire() ? (int)(std::find(m_wires.begin(), m_wires.end(), m->clampedWire()) - m_wires.begin()) : -1;
        ms.append(QJsonObject{{"kind", kinds().value(m->kind)}, {"x", m->pos().x()}, {"y", m->pos().y()}, {"params", pv},
                              {"clamp", wi >= (int)m_wires.size() ? -1 : wi}});
    }
    auto ref = [&](BenchSocket *s) {
        int mi = (int)(std::find(m_modules.begin(), m_modules.end(), s->module) - m_modules.begin());
        int si = (int)(std::find(s->module->sockets.begin(), s->module->sockets.end(), s) - s->module->sockets.begin());
        return QJsonArray{mi, si};
    };
    for (BenchWire *w : m_wires)
        ws.append(QJsonObject{{"a", ref(w->a)}, {"b", ref(w->b)}, {"color", w->color().name()}});
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(QJsonObject{{"voltix_bench", 1}, {"modules", ms}, {"wires", ws}}).toJson());
    return true;
}

bool BenchWindow::loadFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject() || !doc.object().contains("voltix_bench")) return false;
    clearAll();
    std::vector<BenchModule *> made;
    std::vector<int> clampIdx;
    for (const QJsonValue &v : doc.object().value("modules").toArray()) {
        const QJsonObject o = v.toObject();
        BenchModule *m = addModule(o.value("kind").toString(), QPointF(o.value("x").toDouble(), o.value("y").toDouble()));
        if (!m) return false;
        const QJsonArray pv = o.value("params").toArray();
        for (int i = 0; i < pv.size() && i < (int)m->params.size(); ++i)
            m->params[i].value = qBound(m->params[i].min, pv[i].toDouble(), m->params[i].max);
        made.push_back(m);
        clampIdx.push_back(o.value("clamp").toInt(-1));
    }
    auto sock = [&](const QJsonValue &r) -> BenchSocket * {
        const QJsonArray a = r.toArray();
        const int mi = a.at(0).toInt(-1), si = a.at(1).toInt(-1);
        if (mi < 0 || mi >= (int)made.size() || si < 0 || si >= (int)made[mi]->sockets.size()) return nullptr;
        return made[mi]->sockets[si];
    };
    for (const QJsonValue &v : doc.object().value("wires").toArray()) {
        const QJsonObject o = v.toObject();
        connectSockets(sock(o.value("a")), sock(o.value("b")), QColor(o.value("color").toString()));
    }
    for (size_t i = 0; i < made.size(); ++i)
        if (clampIdx[i] >= 0 && clampIdx[i] < (int)m_wires.size()) made[i]->clampTo(m_wires[clampIdx[i]]);
    recompute();
    return true;
}

void BenchWindow::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    if (!m_fitted) { m_fitted = true; m_view->fitInView(QRectF(0, 0, 1700, 1250), Qt::KeepAspectRatio); }
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
    for (BenchModule *m : m_modules) {                       // liaisons internes (bornier...)
        std::vector<std::pair<BenchSocket *, BenchSocket *>> lk;
        m->links(lk);
        for (auto &pr : lk) par[find(pr.first->net)] = find(pr.second->net);
    }

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
    BenchSolution sol;
    sol.all = all;

    // resolution (MNA) en alternatif (phaseurs, 50 Hz) puis en continu (bobines = resistance, condensateurs = ouverts)
    auto solveSys = [&](bool dc, std::vector<bcplx> &out, std::vector<bcplx> *xs = nullptr) -> bool {
        out.assign(std::max(nets, 1), bcplx(0, 0));
        if (D <= 0) return true;
        std::vector<bcplx> A((size_t)D * D, bcplx(0, 0)), b(D, bcplx(0, 0));
        auto at = [&](int r, int c) -> bcplx & { return A[(size_t)r * D + c]; };
        for (int k = 0; k < nn; ++k) at(k, k) += 1e-9;                        // evite les noeuds flottants
        for (const BenchElem &e : el) {
            int i = e.a->net - 1, j = e.b->net - 1;
            if (e.a->net == e.b->net) continue;
            bcplx y = dc ? bcplx(e.ydc >= 0 ? e.ydc : e.y.real(), 0) : e.y;
            if (i >= 0) at(i, i) += y;
            if (j >= 0) at(j, j) += y;
            if (i >= 0 && j >= 0) { at(i, j) -= y; at(j, i) -= y; }
        }
        for (int k = 0; k < ns; ++k) {
            int row = nn + k, ip = src[k].p->net - 1, in = src[k].n->net - 1;
            if (ip >= 0) { at(ip, row) += 1.0; at(row, ip) += 1.0; }
            if (in >= 0) { at(in, row) -= 1.0; at(row, in) -= 1.0; }
            b[row] = dc ? bcplx(src[k].vdc, 0) : src[k].v;
        }
        for (int c = 0; c < D; ++c) {                                          // elimination de Gauss (pivot partiel)
            int piv = c;
            for (int r = c + 1; r < D; ++r) if (std::abs(at(r, c)) > std::abs(at(piv, c))) piv = r;
            if (std::abs(at(piv, c)) < 1e-12) return false;
            if (piv != c) { for (int k = 0; k < D; ++k) std::swap(at(c, k), at(piv, k)); std::swap(b[c], b[piv]); }
            for (int r = c + 1; r < D; ++r) {
                bcplx f = at(r, c) / at(c, c);
                if (f == bcplx(0, 0)) continue;
                for (int k = c; k < D; ++k) at(r, k) -= f * at(c, k);
                b[r] -= f * b[c];
            }
        }
        std::vector<bcplx> x(D);
        for (int r = D - 1; r >= 0; --r) {
            bcplx s = b[r];
            for (int k = r + 1; k < D; ++k) s -= at(r, k) * x[k];
            x[r] = s / at(r, r);
        }
        for (int k = 0; k < nn; ++k) out[k + 1] = x[k];
        if (xs) xs->assign(x.begin() + nn, x.end());
        return true;
    };
    std::vector<bcplx> dcv, xac, xdc;
    const bool ok = solveSys(false, sol.v, &xac) && solveSys(true, dcv, &xdc);
    for (int i = 0; i < N; ++i) all[i]->idx = i;
    {   // ohmmetre : resistance vue entre deux douilles, circuit hors tension (sources = courts-circuits)
        struct R { int a, b; double g; };
        auto rs = std::make_shared<std::vector<R>>();
        for (const BenchElem &e : el) rs->push_back({e.a->net, e.b->net, e.ydc >= 0 ? e.ydc : e.y.real()});
        for (const BenchSrc &s : src) rs->push_back({s.p->net, s.n->net, 1e6});
        const int nets_ = nets;
        sol.rdc = [rs, nets_](const BenchSocket *p, const BenchSocket *q) -> double {
            const int n = nets_ - 1;
            if (p->net == q->net) return 0.0;
            if (n <= 0) return 1e12;
            std::vector<double> A((size_t)n * n, 0.0), b(n, 0.0);
            auto at = [&](int r, int k) -> double & { return A[(size_t)r * n + k]; };
            for (int k = 0; k < n; ++k) at(k, k) += 1e-9;
            for (const R &e : *rs) {
                int i = e.a - 1, j = e.b - 1;
                if (e.a == e.b) continue;
                if (i >= 0) at(i, i) += e.g;
                if (j >= 0) at(j, j) += e.g;
                if (i >= 0 && j >= 0) { at(i, j) -= e.g; at(j, i) -= e.g; }
            }
            if (p->net > 0) b[p->net - 1] += 1.0;
            if (q->net > 0) b[q->net - 1] -= 1.0;
            for (int cc = 0; cc < n; ++cc) {
                int piv = cc;
                for (int r = cc + 1; r < n; ++r) if (std::abs(at(r, cc)) > std::abs(at(piv, cc))) piv = r;
                if (std::abs(at(piv, cc)) < 1e-15) return 1e12;
                if (piv != cc) { for (int k = 0; k < n; ++k) std::swap(at(cc, k), at(piv, k)); std::swap(b[cc], b[piv]); }
                for (int r = cc + 1; r < n; ++r) {
                    const double f = at(r, cc) / at(cc, cc);
                    if (f == 0.0) continue;
                    for (int k = cc; k < n; ++k) at(r, k) -= f * at(cc, k);
                    b[r] -= f * b[cc];
                }
            }
            std::vector<double> x(n);
            for (int r = n - 1; r >= 0; --r) {
                double s = b[r];
                for (int k = r + 1; k < n; ++k) s -= at(r, k) * x[k];
                x[r] = s / at(r, r);
            }
            const double vp = p->net > 0 ? x[p->net - 1] : 0.0, vq = q->net > 0 ? x[q->net - 1] : 0.0;
            return std::abs(vp - vq);
        };
    }
    sol.iin.assign(N, bcplx(0, 0));
    sol.iindc.assign(N, 0.0);
    if (ok) {
        auto vAt = [&](const std::vector<bcplx> &v, int net) { return net < (int)v.size() ? v[net] : bcplx(0, 0); };
        for (const BenchElem &e : el) {
            if (e.a->net == e.b->net) continue;
            const bcplx i = (vAt(sol.v, e.a->net) - vAt(sol.v, e.b->net)) * e.y;
            const double yd = e.ydc >= 0 ? e.ydc : e.y.real();
            const double id = (vAt(dcv, e.a->net) - vAt(dcv, e.b->net)).real() * yd;
            sol.iin[e.a->idx] += i;  sol.iin[e.b->idx] -= i;
            sol.iindc[e.a->idx] += id; sol.iindc[e.b->idx] -= id;
        }
        for (int k = 0; k < ns && k < (int)xac.size() && k < (int)xdc.size(); ++k) {
            sol.iin[src[k].p->idx] += xac[k];  sol.iin[src[k].n->idx] -= xac[k];
            sol.iindc[src[k].p->idx] += xdc[k].real(); sol.iindc[src[k].n->idx] -= xdc[k].real();
        }
    }
    sol.vdc.assign(std::max(nets, 1), 0.0);
    for (size_t k = 0; k < dcv.size() && k < sol.vdc.size(); ++k) sol.vdc[k] = dcv[k].real();

    // 4) affichage
    for (BenchSocket *s : all) {
        bcplx v = sol.at(s);
        QString tip = QStringLiteral("%1 : %2 V  %3 deg (par rapport au neutre)")
                          .arg(s->name).arg(fmt(std::abs(v), 1)).arg(fmt(std::arg(v) * 180.0 / 3.141592653589793, 0));
        if (std::abs(sol.dcAt(s)) > 1e-3) tip += QStringLiteral("   |   continu : %1 V").arg(fmt(sol.dcAt(s), 3));
        s->setToolTip(tip);
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
    m_scene->setSceneRect(-40, -40, 1780, 1330);
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
    edit->addAction(QStringLiteral("Ouvrir un banc..."), this, [this] {
        const QString p = QFileDialog::getOpenFileName(this, QStringLiteral("Ouvrir un banc"), QString(), QStringLiteral("Banc Voltix (*.banc)"));
        if (!p.isEmpty() && !loadFromFile(p)) QMessageBox::warning(this, QStringLiteral("Voltix"), QStringLiteral("Impossible de lire ce banc."));
    });
    edit->addAction(QStringLiteral("Enregistrer le banc..."), this, [this] {
        QString p = QFileDialog::getSaveFileName(this, QStringLiteral("Enregistrer le banc"), QString(), QStringLiteral("Banc Voltix (*.banc)"));
        if (p.isEmpty()) return;
        if (!p.endsWith(".banc")) p += ".banc";
        if (!saveToFile(p)) QMessageBox::warning(this, QStringLiteral("Voltix"), QStringLiteral("Impossible d'ecrire le fichier."));
    });
    edit->addSeparator();
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
    m_scene->onClampChanged = [this] { recompute(); };
    m_scene->onHint = [this](const QString &t) { m_label->setText(t); };
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
    addModule("Pince wattmetre 600 A (F205)");
    addModule("Oscilloscope 2 voies");
    addModule("Sonde differentielle");
    addModule("Rheostat 1400 ohm / 1 A");
    recompute();
}
