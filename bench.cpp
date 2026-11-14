#include "bench.h"
#include <memory>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QSet>
#include <QHash>
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
// Banc vu de face : le pupitre (console + table + caisson des molettes) est au centre ;
// de la place est laissee de chaque cote, au sol, pour les charges et les transformateurs.
static const double kTableW = 3600, kTableH = 1380, kFloorY = 1300;
static const QPointF kBenchPos(700, 500);                // coin haut gauche du module « Pupitre »
static const QRectF kZoneTop(780, 20, 2040, 480);        // sur la tablette de la console : appareils de mesure
static const QRectF kZoneLeft(20, 340, 660, 950);        // a gauche du banc : charges
static const QRectF kZoneRight(2920, 340, 660, 950);     // a droite du banc : transformateurs
static const QRectF kZoneUnder(1130, 890, 1640, 400);    // sous la table, entre le caisson et le pied
static const QRectF kViewBench(640, 0, 2320, 1380);      // vue de depart : le pupitre et sa table

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
    const QString plate = plateHtml();
    const bool form_ = !params.empty() && !plateOnly;
    if (plate.isEmpty() && !form_) return;
    QDialog d;
    d.setWindowTitle(plate.isEmpty() ? title : QStringLiteral("Plaque signaletique - ") + title);
    auto *lay = new QVBoxLayout(&d);
    if (!plate.isEmpty()) {                             // plaque signaletique, couplages
        auto *lab = new QLabel(plate);
        lab->setTextFormat(Qt::RichText);
        lab->setWordWrap(true);
        lab->setStyleSheet(QStringLiteral("QLabel { background:#1c1c1e; color:#f2f2f2; padding:12px; border:2px solid #c8c8c8; }"));
        lay->addWidget(lab);
    }
    auto *form = new QFormLayout;
    lay->addLayout(form);
    std::vector<QDoubleSpinBox *> boxes;
    if (form_)
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
    auto *bb = new QDialogButtonBox(form_ ? QDialogButtonBox::Ok | QDialogButtonBox::Cancel : QDialogButtonBox::Close);
    lay->addWidget(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    if (d.exec() != QDialog::Accepted || !form_) return;
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

// ---- Pupitre : console d'alimentation du banc, table et caisson des autotransformateurs -------------------
// De gauche a droite, comme sur le banc reel : prises PC 230 V, sortie variable monophasee (0-250 V AC ou DC),
// sectionneur, marche / arret general et arret d'urgence, sortie fixe TRI 400 V + N, sortie variable triphasee
// (0-430 V AC ou 0-270 V DC), prises PC 230 V. Sous la table : grande molette = variable triphase,
// petite molette = variable monophase.
class Pupitre : public BenchModule {
public:
    enum { PSect, PAu, PTri, PMono, PGen, PFix, PVarM, PVarT, PSelM, PSelT };
    enum Ctl { CNone, CSect, CAu, CGenOn, CGenOff, CFixOn, CFixOff, CVmOn, CVmOff, CVtOn, CVtOff, CSelM, CSelT, CDialT, CDialM };
    static constexpr double OX = 60, CW = 2080, CH = 320;         // console : origine, largeur, hauteur

    Pupitre() : BenchModule(QStringLiteral("Pupitre d'alimentation"), 2200, 812, QColor(203, 204, 206)) {
        instrument = true;
        setZValue(0.5);                              // les autres modules passent toujours devant
        setFlag(ItemIsMovable, false);               // le pupitre est fixe : c'est le meuble du banc
        setAcceptHoverEvents(true);
        const double y = 222;
        // sortie fixe TRI 400 V + N (les 5 premieres douilles, dans l'ordre de l'ancien pupitre)
        fPe = addSocket("PE", cPE, OX + 1047, y);  fN = addSocket("N", cBlue, OX + 1091, y);
        for (int k = 0; k < 3; ++k) fP[k] = addSocket(QStringLiteral("PH%1").arg(k + 1), cRed, OX + 1135 + 44 * k, y);
        // variable monophase : 0-250 V AC puis 0-250 V DC
        mPe = addSocket("PE", cPE, OX + 256, y);  mN = addSocket("N", cBlue, OX + 300, y);  mP = addSocket("PH1", cRed, OX + 344, y);
        mPlus = addSocket("+", cRed, OX + 635, y);  mMinus = addSocket("-", cBlue, OX + 685, y);
        // variable triphase : 0-430 V AC puis 0-270 V DC
        tPe = addSocket("PE", cPE, OX + 1287, y);  tN = addSocket("N", cBlue, OX + 1331, y);
        for (int k = 0; k < 3; ++k) tP[k] = addSocket(QStringLiteral("PH%1").arg(k + 1), cRed, OX + 1375 + 44 * k, y);
        tPlus = addSocket("+", cRed, OX + 1755, y);  tMinus = addSocket("-", cBlue, OX + 1805, y);

        params = { {"Sectionneur general (1 = ferme)", "", 0, 1, 0, 1, 0},
                   {"Arret d'urgence (1 = enfonce)", "", 0, 1, 0, 1, 0},
                   {"Molette du variable triphase", "%", 0, 100, 0, 1, 0},
                   {"Molette du variable monophase", "%", 0, 100, 0, 1, 0},
                   {"General en marche", "", 0, 1, 0, 1, 0},
                   {"TRI 400 V + N en marche", "", 0, 1, 0, 1, 0},
                   {"Variable monophase en marche", "", 0, 1, 0, 1, 0},
                   {"Variable triphase en marche", "", 0, 1, 0, 1, 0},
                   {"Selecteur du variable monophase (0 = AC, 1 = DC)", "", 0, 1, 0, 1, 0},
                   {"Selecteur du variable triphase (0 = AC, 1 = DC)", "", 0, 1, 0, 1, 0} };

        auto zone = [this](Ctl c, double cx, double cy, double r, const QString &tip) {
            zones.push_back({c, QRectF(OX + cx - r, cy - r, 2 * r, 2 * r), tip});
        };
        zone(CSect, 832, 192, 44, QStringLiteral("Sectionneur general : cliquer pour le fermer ou l'ouvrir. C'est la premiere manoeuvre de mise en service."));
        zone(CAu, 960, 234, 38, QStringLiteral("Arret d'urgence a cle : cliquer pour l'enfoncer (tout s'arrete), cliquer de nouveau pour le deverrouiller."));
        zone(CGenOn, 925, 108, 20, QStringLiteral("MARCHE general : met le pupitre sous tension (sectionneur ferme, arret d'urgence relache)."));
        zone(CGenOff, 995, 108, 20, QStringLiteral("ARRET general : coupe toutes les sorties."));
        zone(CFixOn, 1075, 108, 20, QStringLiteral("MARCHE (cle) de la sortie fixe TRI 400 V + N."));
        zone(CFixOff, 1195, 108, 20, QStringLiteral("ARRET de la sortie fixe TRI 400 V + N."));
        zone(CVmOn, 440, 108, 20, QStringLiteral("MARCHE du variable monophase (reglage : petite molette sous la table)."));
        zone(CVmOff, 510, 108, 20, QStringLiteral("ARRET du variable monophase."));
        zone(CSelM, 475, 222, 20, QStringLiteral("Commutateur AC / DC du variable monophase : sortie 0-250 V AC (PH1 - N) ou 0-250 V DC (+ / -)."));
        zone(CVtOn, 1520, 108, 20, QStringLiteral("MARCHE (cle) du variable triphase (reglage : grande molette sous la table)."));
        zone(CVtOff, 1640, 108, 20, QStringLiteral("ARRET du variable triphase."));
        zone(CSelT, 1580, 222, 20, QStringLiteral("Commutateur AC / DC du variable triphase : sortie 0-430 V AC ou 0-270 V DC (+ / -)."));
    }

    // ---------------------------------------------------------------- etat
    bool on(int i) const { return params[i].value > 0.5; }
    void set(int i, bool v) { params[i].value = v ? 1 : 0; }
    bool powered() const { return on(PSect) && !on(PAu); }
    void normalize() {                               // un arret en amont fait tomber tout ce qui est en aval
        if (!powered()) set(PGen, false);
        if (!on(PGen)) { set(PFix, false); set(PVarM, false); set(PVarT, false); }
    }
    double kT() const { return params[PTri].value / 100.0; }
    double kM() const { return params[PMono].value / 100.0; }

    bool isSupply() const override { return true; }
    bool hasLinks() const override { return true; }
    void links(std::vector<std::pair<BenchSocket *, BenchSocket *>> &l) override {
        l.push_back({fN, mN}); l.push_back({fN, tN});            // neutre commun
        l.push_back({fPe, mPe}); l.push_back({fPe, tPe});        // terre commune
    }
    void sources(std::vector<BenchSrc> &s) override {
        normalize();
        const double a3 = PI2 / 3;
        if (on(PFix))
            for (int k = 0; k < 3; ++k) s.push_back({fP[k], fN, std::polar(230.0, -a3 * k)});
        if (on(PVarT)) {
            if (!on(PSelT)) for (int k = 0; k < 3; ++k) s.push_back({tP[k], tN, std::polar(430.0 / std::sqrt(3.0) * kT(), -a3 * k)});
            else s.push_back({tPlus, tMinus, bcplx(0, 0), 270.0 * kT()});
        }
        if (on(PVarM)) {
            if (!on(PSelM)) s.push_back({mP, mN, std::polar(250.0 * kM(), 0.0)});
            else s.push_back({mPlus, mMinus, bcplx(0, 0), 250.0 * kM()});
        }
    }
    void post(const BenchSolution &sol) override {
        vMac = std::abs(sol.at(mP) - sol.at(mN));          iMac = std::abs(sol.iinAt(mP));
        vMdc = std::abs(sol.dcAt(mPlus) - sol.dcAt(mMinus)); iMdc = std::abs(sol.iinDcAt(mPlus));
        vTac = std::abs(sol.at(tP[0]) - sol.at(tP[1]));
        iTac = std::max({std::abs(sol.iinAt(tP[0])), std::abs(sol.iinAt(tP[1])), std::abs(sol.iinAt(tP[2]))});
        vTdc = std::abs(sol.dcAt(tPlus) - sol.dcAt(tMinus)); iTdc = std::abs(sol.iinDcAt(tPlus));
        QStringList w;
        if (iMac > 5.05) w << QStringLiteral("SURCHARGE 0-250 V AC (%1 A > 5 A)").arg(fmt(iMac, 1));
        if (iMdc > 5.05) w << QStringLiteral("SURCHARGE 0-250 V DC (%1 A > 5 A)").arg(fmt(iMdc, 1));
        if (iTac > 10.1) w << QStringLiteral("SURCHARGE 0-430 V AC (%1 A > 10 A)").arg(fmt(iTac, 1));
        if (iTdc > 20.2) w << QStringLiteral("SURCHARGE 0-270 V DC (%1 A > 20 A)").arg(fmt(iTdc, 1));
        if (!note.isEmpty()) w << note;
        warn = w.join("  -  ");
    }

    // ---------------------------------------------------------------- manoeuvres
    QPointF dialC(bool tri) const { return tri ? QPointF(240, 506) : QPointF(240, 700); }
    double dialR(bool tri) const { return tri ? 82 : 52; }
    Ctl hit(QPointF p, QString *tip = nullptr) const {
        for (bool tri : {true, false})
            if (QLineF(p, dialC(tri)).length() <= dialR(tri) + 6) {
                if (tip) *tip = tri ? QStringLiteral("Grande molette : variable TRIPHASE (%1 %). Molette de la souris ou glisser pour tourner.").arg(fmt(params[PTri].value, 0))
                                    : QStringLiteral("Petite molette : variable MONOPHASE (%1 %). Molette de la souris ou glisser pour tourner.").arg(fmt(params[PMono].value, 0));
                return tri ? CDialT : CDialM;
            }
        for (const Zone &z : zones)
            if (z.r.contains(p)) { if (tip) *tip = z.tip; return z.c; }
        return CNone;
    }
    void act(Ctl c) {
        note.clear();
        auto need = [this](bool ok, const QString &why) { if (!ok) note = why; return ok; };
        switch (c) {
        case CSect: set(PSect, !on(PSect)); break;
        case CAu: set(PAu, !on(PAu)); break;
        case CGenOn:
            if (need(on(PSect), QStringLiteral("Fermer d'abord le sectionneur general"))
                && need(!on(PAu), QStringLiteral("Deverrouiller l'arret d'urgence avant la mise en marche")))
                set(PGen, true);
            break;
        case CGenOff: set(PGen, false); break;
        case CFixOn: if (need(on(PGen), QStringLiteral("Appuyer d'abord sur MARCHE general"))) set(PFix, true); break;
        case CFixOff: set(PFix, false); break;
        case CVmOn: if (need(on(PGen), QStringLiteral("Appuyer d'abord sur MARCHE general"))) set(PVarM, true); break;
        case CVmOff: set(PVarM, false); break;
        case CVtOn: if (need(on(PGen), QStringLiteral("Appuyer d'abord sur MARCHE general"))) set(PVarT, true); break;
        case CVtOff: set(PVarT, false); break;
        case CSelM: set(PSelM, !on(PSelM)); break;
        case CSelT: set(PSelT, !on(PSelT)); break;
        default: break;
        }
        normalize();
        update();
        if (changed) changed();
    }
    void turn(bool tri, QPointF p) {                 // la molette suit le pointeur (0 % en bas a gauche, 100 % en bas a droite)
        const QPointF d = p - dialC(tri);
        double a = qRadiansToDegrees(std::atan2(d.x(), -d.y()));
        a = qBound(-150.0, a, 150.0);
        params[tri ? PTri : PMono].value = qRound((a + 150.0) / 3.0);
        note.clear();
        update();
        if (changed) changed();
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override {
        const Ctl c = e->button() == Qt::LeftButton ? hit(e->pos()) : CNone;
        if (c == CDialT || c == CDialM) { drag = c; turn(c == CDialT, e->pos()); e->accept(); return; }
        if (c != CNone) { act(c); e->accept(); return; }
        BenchModule::mousePressEvent(e);
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override {
        if (drag != CNone) { turn(drag == CDialT, e->pos()); e->accept(); return; }
        BenchModule::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override {
        if (drag != CNone) { drag = CNone; e->accept(); return; }
        BenchModule::mouseReleaseEvent(e);
    }
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override {      // un double-clic reste une manoeuvre, pas une fenetre
        const Ctl c = hit(e->pos());
        if (c != CNone && c != CDialT && c != CDialM) act(c);
        e->accept();
    }
    void wheelEvent(QGraphicsSceneWheelEvent *e) override {
        const Ctl c = hit(e->pos());
        if (c != CDialT && c != CDialM) { e->ignore(); return; }
        BenchParam &p = params[c == CDialT ? PTri : PMono];
        const double step = (e->modifiers() & Qt::ShiftModifier) ? 5 : 1;
        p.value = qBound(0.0, p.value + (e->delta() > 0 ? step : -step), 100.0);
        note.clear();
        update();
        if (changed) changed();
        e->accept();
    }
    void hoverMoveEvent(QGraphicsSceneHoverEvent *e) override {
        QString tip;
        hit(e->pos(), &tip);
        setToolTip(tip);
        setCursor(tip.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    }

public:
    QPainterPath bodyPath() const override {         // zones cliquables : console, plateau, caisson, pied
        QPainterPath b;
        b.addRect(QRectF(OX, 0, CW, CH));
        b.addRect(QRectF(0, CH, w, 56));
        b.addRect(QRectF(70, CH + 56, 340, 424));
        b.addRect(QRectF(w - 110, CH + 56, 50, 424));
        return b;
    }

    // ---------------------------------------------------------------- dessin
    static void px(QPainter *p, int size, bool bold = true) { QFont f = p->font(); f.setPixelSize(size); f.setBold(bold); p->setFont(f); }
    static void plate(QPainter *p, double cx, double cy, const QString &t, bool red = false, int size = 9) {
        px(p, size);
        const QStringList lines = t.split('\n');
        double tw = 0;
        for (const QString &l : lines) tw = std::max(tw, (double)QFontMetricsF(p->font()).horizontalAdvance(l));
        const QRectF r(cx - tw / 2 - 9, cy - 4 - 5.5 * lines.size(), tw + 18, 8 + 11.0 * lines.size());
        p->setPen(QPen(QColor(235, 235, 235), 1));
        p->setBrush(red ? QColor(205, 40, 45) : QColor(22, 22, 24));
        p->drawRect(r);
        p->setPen(Qt::white);
        p->drawText(r, Qt::AlignCenter, t);
    }
    static void bezel(QPainter *p, double cx, double cy) {
        p->setPen(QPen(QColor(20, 20, 22), 1));
        p->setBrush(QColor(58, 60, 66));
        p->drawRect(QRectF(cx - 19, cy - 19, 38, 38));
    }
    static void lamp(QPainter *p, double cx, double cy, bool lit, bool label = true) {     // voyant « EN SERVICE »
        if (label) plate(p, cx, cy - 34, QStringLiteral("EN\nSERVICE"), false, 7);
        bezel(p, cx, cy);
        if (lit) { p->setPen(Qt::NoPen); p->setBrush(QColor(60, 255, 190, 70)); p->drawEllipse(QPointF(cx, cy), 25, 25); }
        p->setPen(QPen(QColor(15, 60, 50), 1.5));
        p->setBrush(lit ? QColor(40, 235, 170) : QColor(0, 120, 95));
        p->drawEllipse(QPointF(cx, cy), 15, 15);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, lit ? 150 : 50));
        p->drawEllipse(QPointF(cx - 5, cy - 5), 5, 4);
    }
    static void pushOn(QPainter *p, double cx, double cy, bool lit) {       // poussoir lumineux vert « MARCHE »
        plate(p, cx, cy - 31, QStringLiteral("MARCHE"), false, 8);
        bezel(p, cx, cy);
        p->setPen(QPen(QColor(200, 205, 210), 3));
        p->setBrush(lit ? QColor(40, 235, 170) : QColor(0, 140, 105));
        p->drawEllipse(QPointF(cx, cy), 14, 14);
    }
    static void pushOff(QPainter *p, double cx, double cy) {                // poussoir rouge « ARRET »
        plate(p, cx, cy - 31, QStringLiteral("ARRET"), true, 8);
        bezel(p, cx, cy);
        p->setPen(QPen(QColor(120, 20, 22), 1.5));
        p->setBrush(QColor(230, 62, 62));
        p->drawEllipse(QPointF(cx, cy), 16, 16);
    }
    static void keySwitch(QPainter *p, double cx, double cy, bool turned) { // bouton a cle « MARCHE »
        plate(p, cx, cy - 31, QStringLiteral("MARCHE"), false, 8);
        bezel(p, cx, cy);
        p->setPen(QPen(QColor(205, 208, 212), 3));
        p->setBrush(QColor(20, 20, 22));
        p->drawEllipse(QPointF(cx, cy), 15, 15);
        p->save();
        p->translate(cx, cy);
        p->rotate(turned ? 45 : -35);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(190, 192, 196));
        p->drawEllipse(QPointF(0, 0), 7, 7);
        p->setBrush(QColor(15, 15, 17));
        p->drawRoundedRect(QRectF(-3, -17, 6, 24), 2, 2);
        p->setBrush(Qt::white);
        p->drawRect(QRectF(-1, -15, 2, 7));
        p->restore();
    }
    static void selector(QPainter *p, double cx, double cy, bool dc) {      // commutateur AC / DC
        plate(p, cx, cy - 31, QStringLiteral("AC      DC"), false, 8);
        p->setPen(QPen(QColor(200, 203, 208), 3));
        p->setBrush(QColor(20, 20, 22));
        p->drawEllipse(QPointF(cx, cy), 16, 16);
        p->save();
        p->translate(cx, cy);
        p->rotate(dc ? 40 : -40);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(12, 12, 14));
        p->drawRoundedRect(QRectF(-5, -20, 10, 40), 4, 4);
        p->setBrush(Qt::white);
        p->drawRect(QRectF(-1.2, -18, 2.4, 16));
        p->restore();
    }
    // appareil a cadre mobile : aiguille pivotant en bas a droite, de l'horizontale (0) a la verticale (pleine echelle)
    static void meter(QPainter *p, double x, double y, const QString &letter, double max, double major, double value) {
        const QRectF r(OX + x, y, 80, 80);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(18, 18, 20));
        p->drawRect(r);
        p->setBrush(QColor(246, 246, 240));
        p->drawRect(r.adjusted(6, 6, -6, -6));
        const QPointF c(r.right() - 16, r.bottom() - 16);
        const double R = 54;
        auto pt = [&](double frac, double rad) { const double a = (180.0 - 90.0 * frac) * 3.14159265 / 180.0; return QPointF(c.x() + rad * std::cos(a), c.y() - rad * std::sin(a)); };
        p->setPen(QPen(QColor(20, 20, 20), 1));
        px(p, 8, false);
        const int n = qRound(max / (major / 5.0));
        for (int i = 0; i <= n; ++i) {
            const double v = i * major / 5.0, f = v / max;
            const bool big = (i % 5) == 0;
            p->drawLine(pt(f, R), pt(f, R - (big ? 7 : 4)));
            if (big && v > 0) {
                const QPointF t = pt(f, R - 15);
                p->drawText(QRectF(t.x() - 14, t.y() - 6, 28, 12), Qt::AlignCenter, QString::number(v, 'f', 0));
            }
        }
        px(p, 14);
        p->drawText(QRectF(r.left() + 9, r.top() + 7, 20, 18), Qt::AlignLeft | Qt::AlignVCenter, letter);
        p->setPen(QPen(QColor(15, 15, 15), 2));
        p->drawLine(pt(std::min(value / max, 1.04), 10), pt(std::min(value / max, 1.04), R - 2));
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(238, 234, 214));
        p->drawRect(QRectF(r.right() - 36, r.bottom() - 34, 30, 28));
        p->setBrush(QColor(20, 20, 20));
        p->drawEllipse(c, 4.5, 4.5);
    }
    static void outlet(QPainter *p, double x, double y) {                    // double prise 2P + T
        p->setPen(QPen(QColor(150, 150, 150), 1));
        p->setBrush(QColor(250, 250, 250));
        p->drawRoundedRect(QRectF(OX + x, y, 130, 62), 3, 3);
        for (int k = 0; k < 2; ++k) {
            const QPointF c(OX + x + 34 + 62 * k, y + 31);
            p->setPen(QPen(QColor(205, 205, 205), 1));
            p->setBrush(QColor(238, 238, 238));
            p->drawEllipse(c, 24, 24);
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(25, 25, 25));
            p->drawEllipse(c + QPointF(-9, -6), 3, 3);
            p->drawEllipse(c + QPointF(7, 10), 3, 3);
            p->setPen(QPen(QColor(150, 150, 150), 3, Qt::SolidLine, Qt::RoundCap));
            p->drawLine(c + QPointF(4, -6), c + QPointF(12, -9));
        }
    }
    void dial(QPainter *p, bool tri) const {
        const QPointF c = dialC(tri);
        const double R = dialR(tri), v = params[tri ? PTri : PMono].value;
        auto pt = [&](double deg, double rad) { const double a = deg * 3.14159265 / 180.0; return QPointF(c.x() + rad * std::sin(a), c.y() - rad * std::cos(a)); };
        p->setPen(QPen(QColor(10, 10, 10), 2));
        p->setBrush(QColor(24, 24, 26));
        p->drawEllipse(c, R, R);
        px(p, tri ? 9 : 7, false);
        for (int i = 0; i <= 50; ++i) {
            const double a = -150 + 6.0 * i;
            p->setPen(QPen(QColor(235, 235, 235), (i % 5) == 0 ? 1.6 : 0.8));
            p->drawLine(pt(a, R - 3), pt(a, R - ((i % 5) == 0 ? 11 : 7)));
            if ((i % 10) == 0) {
                const QPointF t = pt(a, R - 20);
                p->drawText(QRectF(t.x() - 12, t.y() - 6, 24, 12), Qt::AlignCenter, QString::number(i * 2));
            }
        }
        const double a = -150 + 3.0 * v, rk = R * 0.52;
        p->setPen(QPen(QColor(5, 5, 5), 2));
        p->setBrush(QColor(42, 42, 46));
        p->drawEllipse(c, rk, rk);
        p->setBrush(QColor(30, 30, 33));
        p->drawEllipse(c, rk * 0.62, rk * 0.62);
        p->setPen(QPen(Qt::white, tri ? 4 : 3, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(pt(a, rk * 0.25), pt(a, rk - 3));                          // repere du bouton
        p->setPen(Qt::NoPen);
        p->setBrush(Qt::white);
        QPolygonF idx;
        idx << pt(a, rk + R * 0.15) << pt(a - 10, rk - 1) << pt(a + 10, rk - 1);
        p->drawPolygon(idx);
        p->setPen(QColor(70, 72, 78));
        px(p, 11);
        p->drawText(QRectF(c.x() - 150, c.y() + R + 6, 300, 16), Qt::AlignCenter,
                    tri ? QStringLiteral("VARIABLE TRIPHASE : %1 %").arg(fmt(v, 0)) : QStringLiteral("VARIABLE MONOPHASE : %1 %").arg(fmt(v, 0)));
    }
    static void earth(QPainter *p, double cx, double cy) {
        p->setPen(QPen(Qt::white, 1.4));
        p->drawLine(QPointF(cx, cy - 5), QPointF(cx, cy - 1));
        p->drawLine(QPointF(cx - 7, cy - 1), QPointF(cx + 7, cy - 1));
        p->drawLine(QPointF(cx - 4.5, cy + 2), QPointF(cx + 4.5, cy + 2));
        p->drawLine(QPointF(cx - 2, cy + 5), QPointF(cx + 2, cy + 5));
    }

    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override {
        p->setRenderHint(QPainter::Antialiasing);
        const QColor frame(84, 86, 94), face(203, 204, 206), steel(196, 200, 204), dark(52, 54, 60);

        // ---- table : plateau, bandeau perfore, caisson des autotransformateurs, pied, traverse
        const double ty = CH;
        p->setPen(QPen(QColor(120, 122, 126), 1));
        p->setBrush(dark);
        p->drawRect(QRectF(410, ty + 300, w - 520, 26));                       // traverse
        p->setBrush(steel);
        p->drawRect(QRectF(w - 110, ty + 56, 50, 424));                        // pied droit
        p->drawRect(QRectF(70, ty + 56, 340, 424));                            // caisson
        p->setBrush(dark);
        p->drawRect(QRectF(30, ty + 26, w - 60, 30));                          // bandeau
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(225, 226, 228));
        for (double x = 110; x < w - 80; x += 130) p->drawEllipse(QPointF(x, ty + 41), 5, 5);
        p->setPen(QPen(QColor(20, 20, 22), 1));
        p->setBrush(QColor(240, 238, 230));
        p->drawRect(QRectF(0, ty, w, 20));                                     // plateau
        p->setBrush(QColor(20, 20, 22));
        p->drawRect(QRectF(0, ty + 20, w, 6));                                 // chant noir
        dial(p, true);
        dial(p, false);

        // ---- console
        p->setPen(QPen(isSelected() ? QColor(40, 110, 220) : QColor(40, 40, 44), isSelected() ? 3 : 1));
        p->setBrush(frame);
        p->drawRect(QRectF(OX, 0, CW, CH));
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(34, 34, 38));
        p->drawRect(QRectF(OX - 6, 0, CW + 12, 34));                           // tablette noire (dessus : appareils de mesure)
        p->setBrush(face);
        p->drawRect(QRectF(OX + 20, 50, CW - 40, CH - 68));

        const bool gen = on(PGen);
        // A - prises PC 230 V (gauche)
        lamp(p, OX + 115, 108, gen);
        plate(p, OX + 115, 160, QStringLiteral("PC 230V"));
        outlet(p, 50, 186);
        // B - variable monophase, alternatif
        meter(p, 215, 70, "V", 250, 100, vMac);
        meter(p, 305, 70, "A", 5, 2, iMac);
        plate(p, OX + 300, 165, QStringLiteral("0-250V AC"));
        // C - commandes du variable monophase
        pushOn(p, OX + 440, 108, on(PVarM));
        pushOff(p, OX + 510, 108);
        plate(p, OX + 475, 150, QStringLiteral("VARIABLE"));
        lamp(p, OX + 420, 222, on(PVarM) && !on(PSelM));
        selector(p, OX + 475, 222, on(PSelM));
        lamp(p, OX + 530, 222, on(PVarM) && on(PSelM));
        // D - variable monophase, continu
        meter(p, 575, 70, "V", 250, 100, vMdc);
        meter(p, 665, 70, "A", 5, 2, iMdc);
        plate(p, OX + 660, 165, QStringLiteral("0-250V DC"));
        plate(p, OX + 660, 276, QStringLiteral("NE RACCORDER\nQU'UN SEUL APPAREIL"), false, 8);
        // E - sectionneur general
        {
            const QPointF c(OX + 832, 192);
            p->setPen(QPen(QColor(20, 20, 20), 3));
            p->setBrush(QColor(250, 215, 30));
            p->drawRoundedRect(QRectF(c.x() - 42, c.y() - 42, 84, 84), 4, 4);
            p->setPen(QPen(QColor(20, 20, 20), 1.5));
            px(p, 10);
            p->drawText(QRectF(c.x() - 40, c.y() - 41, 20, 14), Qt::AlignCenter, "O");
            p->drawText(QRectF(c.x() + 20, c.y() - 41, 20, 14), Qt::AlignCenter, "I");
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(225, 60, 60));
            p->drawEllipse(c, 30, 30);
            p->save();
            p->translate(c);
            p->rotate(on(PSect) ? 45 : -45);
            p->setBrush(QColor(200, 45, 48));
            p->drawRoundedRect(QRectF(-9, -40, 18, 62), 6, 6);
            p->setBrush(QColor(245, 245, 240));
            p->drawRoundedRect(QRectF(-4, -38, 8, 34), 2, 2);
            p->restore();
        }
        // F - marche / arret general, arret d'urgence
        pushOn(p, OX + 925, 108, gen);
        pushOff(p, OX + 995, 108);
        plate(p, OX + 960, 150, QStringLiteral("GENERAL"));
        {
            const QPointF c(OX + 960, 234);
            p->setPen(QPen(QColor(150, 120, 0), 1));
            p->setBrush(QColor(250, 215, 30));
            p->drawEllipse(c, 37, 37);
            px(p, 9);
            p->setPen(QColor(20, 20, 20));
            p->drawText(QRectF(c.x() - 37, c.y() - 36, 74, 13), Qt::AlignCenter, QStringLiteral("ARRET"));
            const double rr = on(PAu) ? 21 : 25;
            p->setPen(QPen(QColor(120, 20, 22), 1.5));
            p->setBrush(on(PAu) ? QColor(190, 40, 44) : QColor(232, 66, 66));
            p->drawEllipse(c + QPointF(0, 4), rr, rr);
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(200, 202, 206));
            p->drawEllipse(c + QPointF(0, 4), 7, 7);
            p->setBrush(QColor(30, 30, 30));
            p->save();
            p->translate(c + QPointF(0, 4));
            p->rotate(on(PAu) ? 90 : 0);
            p->drawRect(QRectF(-1.2, -5, 2.4, 10));
            p->restore();
        }
        // G - sortie fixe TRI 400 V + N
        keySwitch(p, OX + 1075, 108, on(PFix));
        lamp(p, OX + 1135, 108, on(PFix));
        pushOff(p, OX + 1195, 108);
        plate(p, OX + 1135, 160, QStringLiteral("TRI 400V+N"));
        // H - variable triphase, alternatif
        meter(p, 1290, 70, "V", 500, 200, vTac);
        meter(p, 1380, 70, "A", 10, 5, iTac);
        plate(p, OX + 1375, 165, QStringLiteral("0-430V AC"));
        // I - commandes du variable triphase
        keySwitch(p, OX + 1520, 108, on(PVarT));
        lamp(p, OX + 1580, 108, on(PVarT));
        pushOff(p, OX + 1640, 108);
        plate(p, OX + 1580, 150, QStringLiteral("VARIABLE"));
        lamp(p, OX + 1525, 222, on(PVarT) && !on(PSelT));
        selector(p, OX + 1580, 222, on(PSelT));
        lamp(p, OX + 1635, 222, on(PVarT) && on(PSelT));
        // J - variable triphase, continu
        meter(p, 1695, 70, "V", 300, 100, vTdc);
        meter(p, 1785, 70, "A", 20, 5, iTdc);
        plate(p, OX + 1780, 165, QStringLiteral("0-270V DC"));
        plate(p, OX + 1780, 276, QStringLiteral("NE RACCORDER\nQU'UN SEUL APPAREIL"), false, 8);
        // K - prises PC 230 V (droite)
        plate(p, OX + 1975, 160, QStringLiteral("PC 230V"));
        outlet(p, 1910, 186);

        // etiquettes des douilles
        for (BenchSocket *s : sockets) {
            const QPointF c = s->pos() + QPointF(0, 26);
            if (s->name == "PE") { plate(p, c.x(), c.y(), QStringLiteral("    "), false, 8); earth(p, c.x(), c.y()); }
            else plate(p, c.x(), c.y(), s->name == "-" ? QStringLiteral(" - ") : s->name == "+" ? QStringLiteral(" + ") : s->name, false, 8);
        }

        if (!warn.isEmpty()) {
            px(p, 12);
            p->setPen(QColor(255, 120, 110));
            p->drawText(QRectF(OX, 8, CW, 20), Qt::AlignCenter, warn);
        }
    }

    struct Zone { Ctl c; QRectF r; QString tip; };
    std::vector<Zone> zones;
    Ctl drag = CNone;
    QString note;
    double vMac = 0, iMac = 0, vMdc = 0, iMdc = 0, vTac = 0, iTac = 0, vTdc = 0, iTdc = 0;
    BenchSocket *fPe, *fN, *fP[3], *mPe, *mN, *mP, *mPlus, *mMinus, *tPe, *tN, *tP[3], *tPlus, *tMinus;
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
    // Loi des noeuds : on retire le fil w, puis on parcourt tout ce qui reste relie a son extremite « a »
    // (autres fils, liaisons internes des appareils). Le courant du fil est la somme des courants qui
    // entrent dans les appareils de ce cote. Si l'on retombe sur l'extremite « b », le fil fait partie
    // d'une boucle de fils : son courant est indetermine.
    QSet<BenchSocket *> seen;
    std::vector<BenchSocket *> todo{w->a};
    seen.insert(w->a);
    QHash<BenchModule *, std::vector<std::pair<BenchSocket *, BenchSocket *>>> linkCache;
    while (!todo.empty()) {
        BenchSocket *x = todo.back(); todo.pop_back();
        auto push = [&](BenchSocket *y) { if (!seen.contains(y)) { seen.insert(y); todo.push_back(y); } };
        for (BenchWire *o : x->wires) if (o != w) push(o->a == x ? o->b : o->a);
        if (x->module->hasLinks()) {
            if (!linkCache.contains(x->module)) x->module->links(linkCache[x->module]);
            for (const auto &pr : linkCache[x->module]) { if (pr.first == x) push(pr.second); if (pr.second == x) push(pr.first); }
        }
    }
    if (seen.contains(w->b)) return false;
    bcplx sum(0, 0); double sumDc = 0;
    for (BenchSocket *x : seen) { sum += s.iinAt(x); sumDc += s.iinDcAt(x); }
    I = -sum; Idc = -sumDc;                              // compte de a vers b
    if (w->b->module->isSupply() && !w->a->module->isSupply()) { I = -I; Idc = -Idc; }
    return true;
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
        else if (st == 2) { main_ = "?"; warn = QStringLiteral("FIL AMBIGU"); sub_ = QStringLiteral("fil dans une boucle"); }
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

// ---- Transformateurs : enroulements accessibles (bornes U1-U2 / u1-u2), a coupler en etoile ou en triangle ----
// Modele : deux enroulements couples (R + L propres, mutuelle M), inverses en matrice d'admittance.
struct TrafoModel {
    double sn, u1, u2, ucc, ur, i0;      // VA, V, V, %, %, %
    double r1 = 0, r2 = 0;
    bcplx y11, y12, y22;
    void build() {
        const double w = PI2 * BenchWindow::kFreq, n = u2 / u1;
        const double zb = u1 * u1 / sn, xcc = std::sqrt(std::max(1e-12, std::pow(ucc / 100.0 * zb, 2) - std::pow(ur / 100.0 * zb, 2)));
        const double i1n = sn / u1, xm = u1 / std::max(1e-6, i0 / 100.0 * i1n);
        r1 = ur / 100.0 * zb / 2; r2 = r1 * n * n;
        const double lm = xm / w, ls1 = xcc / 2 / w, ls2 = xcc / 2 / w * n * n;
        const bcplx z11(r1, w * (lm + ls1)), z22(r2, w * (n * n * lm + ls2)), z12(0, w * n * lm);
        const bcplx det = z11 * z22 - z12 * z12;
        y11 = z22 / det; y22 = z11 / det; y12 = -z12 / det;
    }
    // stampe 4 noeuds [p1,p2,s1,s2] en admittances deux a deux
    void stamp(BenchSocket *p1, BenchSocket *p2, BenchSocket *s1, BenchSocket *s2, std::vector<BenchElem> &el) const {
        BenchSocket *nd[4] = {p1, p2, s1, s2};
        const double B[2][4] = {{1, -1, 0, 0}, {0, 0, 1, -1}};
        const bcplx Y[2][2] = {{y11, y12}, {y12, y22}};
        bcplx Y4[4][4];
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                bcplx s(0, 0);
                for (int a = 0; a < 2; ++a) for (int b = 0; b < 2; ++b) s += B[a][i] * Y[a][b] * B[b][j];
                Y4[i][j] = s;
            }
        for (int i = 0; i < 4; ++i)
            for (int j = i + 1; j < 4; ++j) {
                double ydc = -1;
                if (i == 0 && j == 1) ydc = 1.0 / std::max(r1, 1e-3);
                else if (i == 2 && j == 3) ydc = 1.0 / std::max(r2, 1e-3);
                else ydc = 0.0;
                el.push_back({nd[i], nd[j], -Y4[i][j], ydc});
            }
    }
    bcplx i1(bcplx v1, bcplx v2) const { return y11 * v1 + y12 * v2; }
    bcplx i2(bcplx v1, bcplx v2) const { return y12 * v1 + y22 * v2; }
};

class Trafo : public BenchModule {
public:
    explicit Trafo(bool tri)
        : BenchModule(tri ? QStringLiteral("Transformateur triphase (3 enroulements)") : QStringLiteral("Transformateur monophase"),
                      tri ? 560 : 300, 270, QColor(120, 128, 138)), three(tri)
    {
        const int n = tri ? 3 : 1;
        const double x0 = tri ? 30 : 0, dx = tri ? 170 : 0;
        const char *pn[3][2] = {{"U1", "U2"}, {"V1", "V2"}, {"W1", "W2"}};
        const char *sn[3][2] = {{"u1", "u2"}, {"v1", "v2"}, {"w1", "w2"}};
        for (int k = 0; k < n; ++k) {
            const double cx = tri ? x0 + 70 + dx * k : w / 2.0;
            const char *a = tri ? pn[k][0] : "U1", *b = tri ? pn[k][1] : "U2";
            const char *c = tri ? sn[k][0] : "u1", *d = tri ? sn[k][1] : "u2";
            p1[k] = addSocket(a, cRed, cx - 30, 55);
            p2[k] = addSocket(b, cBlack, cx + 30, 55);
            s1[k] = addSocket(c, cRed, cx - 30, h - 62);
            s2[k] = addSocket(d, cBlack, cx + 30, h - 62);
        }
        params = { {"Puissance apparente", "VA", 50, 20000, tri ? 3000.0 : 1000.0, 50, 0},
                   {"Tension primaire (par enroulement)", "V", 10, 1000, 230, 5, 0},
                   {"Tension secondaire (par enroulement)", "V", 5, 1000, tri ? 230.0 : 115.0, 5, 0},
                   {"Tension de court-circuit", "%", 1, 20, 5, 0.5, 1},
                   {"Pertes Joule nominales", "%", 0.2, 10, 2, 0.1, 1},
                   {"Courant a vide", "%", 0.5, 20, 3, 0.5, 1} };
    }
    TrafoModel model() const {
        TrafoModel m{params[0].value / (three ? 3.0 : 1.0), params[1].value, params[2].value, params[3].value, 0, params[5].value};
        m.ur = std::min(params[4].value, params[3].value * 0.95);
        m.build();
        return m;
    }
    void elements(std::vector<BenchElem> &el) override {
        const TrafoModel m = model();
        for (int k = 0; k < (three ? 3 : 1); ++k) m.stamp(p1[k], p2[k], s1[k], s2[k], el);
    }
    void post(const BenchSolution &sol) override {
        const TrafoModel m = model();
        double ip = 0, is = 0;
        for (int k = 0; k < (three ? 3 : 1); ++k) {
            const bcplx v1 = sol.at(p1[k]) - sol.at(p2[k]), v2 = sol.at(s1[k]) - sol.at(s2[k]);
            ip = std::max(ip, std::abs(m.i1(v1, v2)));
            is = std::max(is, std::abs(m.i2(v1, v2)));
        }
        const double sn = params[0].value / (three ? 3.0 : 1.0);
        i1n = sn / params[1].value; i2n = sn / params[2].value;
        i1 = ip; i2 = is;
        warn = (ip > 1.1 * i1n || is > 1.1 * i2n) ? QStringLiteral("SURCHARGE  I1 = %1 A  I2 = %2 A").arg(fmt(ip, 1)).arg(fmt(is, 1)) : QString();
    }
    void paintExtra(QPainter *p) override {
        p->setPen(QPen(QColor(235, 235, 235), 2));
        p->setBrush(Qt::NoBrush);
        for (int k = 0; k < (three ? 3 : 1); ++k) {
            const double cx = (p1[k]->pos().x() + p2[k]->pos().x()) / 2;
            for (int side = 0; side < 2; ++side) {
                const double yc = side == 0 ? 112 : h - 112, ys = side == 0 ? 55 : h - 62;
                p->drawLine(QPointF(cx - 30, ys), QPointF(cx - 30, yc));
                p->drawLine(QPointF(cx + 30, ys), QPointF(cx + 30, yc));
                QPainterPath pa; pa.moveTo(cx - 30, yc);
                for (int i = 0; i < 6; ++i) pa.arcTo(QRectF(cx - 30 + i * 10, yc - 8, 10, 16), 180, -180);
                p->drawPath(pa);
            }
            p->drawLine(QPointF(cx - 34, h / 2.0 - 6), QPointF(cx + 34, h / 2.0 - 6));
            p->drawLine(QPointF(cx - 34, h / 2.0 + 2), QPointF(cx + 34, h / 2.0 + 2));
        }
        QFont f = p->font(); f.setPointSize(8); p->setFont(f);
        p->setPen(QColor(240, 240, 240));
        p->drawText(QRectF(6, 24, w - 12, 16),Qt::AlignCenter,
                    QStringLiteral("%1 VA   %2 V / %3 V   ucc %4 %").arg(fmt(params[0].value, 0)).arg(fmt(params[1].value, 0))
                        .arg(fmt(params[2].value, 0)).arg(fmt(params[3].value, 1)));
        p->drawText(QRectF(6, h - 32, w - 12, 14), Qt::AlignCenter,
                    QStringLiteral("I1 = %1 A (nom. %2)    I2 = %3 A (nom. %4)").arg(fmt(i1, 2)).arg(fmt(i1n, 2)).arg(fmt(i2, 2)).arg(fmt(i2n, 2)));
    }
    QString plateHtml() const override {
        const double sn = params[0].value, u1 = params[1].value, u2 = params[2].value, k = three ? 3.0 : 1.0;
        return QStringLiteral("<h3>TRANSFORMATEUR %1</h3>"
            "<table border='1' cellspacing='0' cellpadding='4'>"
            "<tr><td>Puissance</td><td><b>%2 kVA</b></td><td>Phases</td><td><b>%3</b></td><td>Frequence</td><td><b>50 Hz</b></td></tr>"
            "<tr><td>Primaire (par enroulement)</td><td><b>%4 V</b></td><td colspan='2'>Courant nominal</td><td colspan='2'><b>%5 A</b></td></tr>"
            "<tr><td>Secondaire (par enroulement)</td><td><b>%6 V</b></td><td colspan='2'>Courant nominal</td><td colspan='2'><b>%7 A</b></td></tr>"
            "<tr><td>Tension de court-circuit</td><td><b>%8 %</b></td><td colspan='2'>Courant a vide</td><td colspan='2'><b>%9 %</b></td></tr></table>"
            "<p>%10</p><p>Les valeurs de la plaque se modifient ci-dessous.</p>")
            .arg(three ? "TRIPHASE" : "MONOPHASE").arg(fmt(sn / 1000.0, 2)).arg(three ? "TRI" : "MONO")
            .arg(fmt(u1, 0)).arg(fmt(sn / k / u1, 2)).arg(fmt(u2, 0)).arg(fmt(sn / k / u2, 2)).arg(fmt(params[3].value, 1)).arg(fmt(params[5].value, 1))
            .arg(three ? QStringLiteral("Couplage a realiser par fils : etoile = U2 V2 W2 (ou u2 v2 w2) ensemble ; triangle = U2 - V1, V2 - W1, W2 - U1. "
                                        "En etoile la tension entre phases vaut racine de 3 fois la tension d'enroulement.")
                         : QStringLiteral("Primaire U1 - U2, secondaire u1 - u2."));
    }
    bool three;
    BenchSocket *p1[3], *p2[3], *s1[3], *s2[3];
    double i1 = 0, i2 = 0, i1n = 0, i2n = 0;
};

// ---- Chariots de charges : commutateurs en facade, 6 bornes de couplage, entrees TRIPHASE / MONOPHASE --------
// Les elements sont entre U1-U2, V1-V2, W1-W2 ; les bornes sont disposees comme une plaque a bornes
// (U1 V1 W1 au-dessus, W2 U2 V2 en dessous) : barrette horizontale en bas = etoile, 3 barrettes verticales = triangle.
// Entrees TRIPHASE reliees a U1 V1 W1 ; entree MONOPHASE : rouge = U1, bleu = U2.
class Cart : public BenchModule {
public:
    struct Sw { QPointF c; QStringList pos; std::vector<double> pct; int param; };
    Cart(const QString &t, const QColor &col) : BenchModule(t, 320, 450, col) {
        setAcceptHoverEvents(true);
        plateOnly = true;
    }
    void terminals(const QColor &triCol) {
        const char *top[3] = {"U1", "V1", "W1"}, *bot[3] = {"W2", "U2", "V2"};
        for (int k = 0; k < 3; ++k) a1[k] = addSocket(top[k], QColor(120, 122, 128), 110 + 50 * k, 292);
        for (int k = 0; k < 3; ++k) bsock[k] = addSocket(bot[k], QColor(120, 122, 128), 110 + 50 * k, 332);
        a2[0] = bsock[1]; a2[1] = bsock[2]; a2[2] = bsock[0];              // U2, V2, W2
        pe = addSocket("PE", cPE, 40, 372);
        for (int k = 0; k < 3; ++k) tri[k] = addSocket(QStringLiteral("L%1").arg(k + 1), triCol, 70 + 40 * k, 412);
        mB = addSocket("N", cBlue, 230, 412);
        mR = addSocket("PH", cRed, 280, 412);
    }
    bool hasLinks() const override { return true; }
    void links(std::vector<std::pair<BenchSocket *, BenchSocket *>> &l) override {
        for (int k = 0; k < 3; ++k) l.push_back({tri[k], a1[k]});
        l.push_back({mR, a1[0]});
        l.push_back({mB, a2[0]});
    }
    double percent() const {
        double s = 0;
        for (const Sw &q : sw) s += q.pct[qBound(0, (int)params[q.param].value, (int)q.pct.size() - 1)];
        return s;
    }
    void addSwitch(QPointF c, const QStringList &pos, std::vector<double> pct) {
        const int n = (int)pos.size();
        params.push_back({QStringLiteral("Commutateur %1").arg(sw.size() + 1), "", 0.0, double(n - 1), 0.0, 1.0, 0});
        sw.push_back({c, pos, std::move(pct), (int)params.size() - 1});
    }
    static void drawSwitch(QPainter *p, const Sw &q, int pos, bool dark) {
        p->setPen(QPen(QColor(20, 20, 20), 1.5));
        p->setBrush(dark ? QColor(28, 28, 30) : QColor(236, 236, 232));
        p->drawRoundedRect(QRectF(q.c.x() - 36, q.c.y() - 36, 72, 72), 6, 6);
        QFont f = p->font(); f.setPixelSize(8); f.setBold(true); p->setFont(f);
        const int n = (int)q.pos.size();
        // positions reparties autour du bouton : ARRET en haut, puis dans le sens horaire
        for (int i = 0; i < n; ++i) {
            const double a = (n == 2 ? 90.0 * i : 90.0 * i) * 3.14159265 / 180.0;
            const QPointF t(q.c.x() + 27 * std::sin(a), q.c.y() - 27 * std::cos(a));
            p->setPen(Qt::NoPen);
            p->setBrush(i == pos ? QColor(255, 235, 120) : QColor(245, 245, 245));
            p->drawRoundedRect(QRectF(t.x() - 15, t.y() - 6, 30, 12), 3, 3);
            p->setPen(QColor(20, 20, 20));
            p->drawText(QRectF(t.x() - 16, t.y() - 6, 32, 12), Qt::AlignCenter, q.pos[i]);
        }
        p->save();
        p->translate(q.c);
        p->rotate(90.0 * pos);
        p->setPen(QPen(QColor(10, 10, 10), 1));
        p->setBrush(dark ? QColor(60, 60, 64) : QColor(170, 172, 176));
        p->drawEllipse(QPointF(0, 0), 15, 15);
        p->setBrush(dark ? QColor(20, 20, 22) : QColor(120, 122, 126));
        QPolygonF h; h << QPointF(0, -22) << QPointF(6, -4) << QPointF(4, 14) << QPointF(-4, 14) << QPointF(-6, -4);
        p->drawPolygon(h);
        p->setBrush(QColor(190, 160, 60));
        p->drawEllipse(QPointF(0, 0), 3, 3);
        p->restore();
    }
    int swAt(QPointF p) const {
        for (size_t i = 0; i < sw.size(); ++i)
            if (QRectF(sw[i].c.x() - 36, sw[i].c.y() - 36, 72, 72).contains(p)) return (int)i;
        return -1;
    }
    void paintTerminals(QPainter *p, const QString &ratingLine) {
        p->setPen(QPen(QColor(235, 235, 235), 1));
        p->setBrush(QColor(22, 22, 24));
        p->drawRect(QRectF(60, 244, 200, 26));
        QFont f = p->font(); f.setPixelSize(9); f.setBold(true); p->setFont(f);
        p->setPen(Qt::white);
        p->drawText(QRectF(60, 244, 200, 26), Qt::AlignCenter, ratingLine);
        f.setPixelSize(8); p->setFont(f);
        p->setPen(body.lightness() < 120 ? QColor(240, 240, 240) : QColor(25, 25, 25));
        p->drawText(QRectF(50, 384, 120, 12), Qt::AlignCenter, QStringLiteral("TRIPHASE"));
        p->drawText(QRectF(205, 384, 100, 12), Qt::AlignCenter, QStringLiteral("MONOPHASE"));
        // roulettes
        p->setPen(QPen(QColor(40, 40, 40), 2));
        p->setBrush(QColor(225, 70, 70));
        p->drawEllipse(QPointF(50, h - 2), 10, 10);
        p->drawEllipse(QPointF(w - 50, h - 2), 10, 10);
    }
    QString couplingHtml(const QString &head, const QString &u) const {
        return head + QStringLiteral(
            "<p><b>Bornes de couplage</b> (3 elements : U1-U2, V1-V2, W1-W2)</p>"
            "<pre>   U1   V1   W1\n   W2   U2   V2</pre>"
            "<table border='1' cellspacing='0' cellpadding='4'>"
            "<tr><th>Reseau</th><th>Couplage</th><th>Barrettes (fils) a poser</th><th>Alimentation</th></tr>"
            "<tr><td>TRI 380 / 400 V</td><td>etoile</td><td>W2 - U2 - V2 (rangee du bas)</td><td>TRIPHASE L1 L2 L3</td></tr>"
            "<tr><td>TRI 220 / 230 V</td><td>triangle</td><td>U1 - W2, V1 - U2, W1 - V2 (verticales)</td><td>TRIPHASE L1 L2 L3</td></tr>"
            "<tr><td>MONO %1 220 / 230 V</td><td>3 elements en parallele</td><td>U1 - V1 - W1 et W2 - U2 - V2</td><td>MONOPHASE PH / N</td></tr>"
            "<tr><td>MONO 127 V</td><td>parallele</td><td>comme ci-dessus</td><td>puissance x 1/3</td></tr>"
            "<tr><td>MONO 110 V</td><td>parallele</td><td>comme ci-dessus</td><td>puissance x 1/4</td></tr>"
            "</table>"
            "<p>Reglage actuel des commutateurs : <b>%2 %</b> de la puissance.</p>").arg(u).arg(fmt(percent(), 0));
    }
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override {
        const int i = e->button() == Qt::LeftButton ? swAt(e->pos()) : -1;
        if (i < 0) { BenchModule::mousePressEvent(e); return; }
        BenchParam &pr = params[sw[i].param];
        pr.value = (int(pr.value) + 1) % (int)sw[i].pos.size();
        update();
        if (changed) changed();
        e->accept();
    }
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override {
        if (swAt(e->pos()) >= 0) { mousePressEvent(e); return; }       // double-clic sur un commutateur = deux crans
        BenchModule::mouseDoubleClickEvent(e);
    }
    void hoverMoveEvent(QGraphicsSceneHoverEvent *e) override {
        const bool onSw = swAt(e->pos()) >= 0;
        setToolTip(onSw ? QStringLiteral("Commutateur : cliquer pour passer a la position suivante.")
                        : QStringLiteral("Double-clic : plaque signaletique et couplages."));
        setCursor(onSw ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
public:
    std::vector<Sw> sw;
    BenchSocket *a1[3], *a2[3], *bsock[3], *pe, *tri[3], *mB, *mR;
};

class ResCart : public Cart {                        // charge resistive 4 kW
public:
    ResCart() : Cart(QStringLiteral("Charge resistive 4 kW"), QColor(150, 128, 92)) {
        for (double x : {110.0, 210.0}) addSwitch(QPointF(x, 78), {"ARRET", "5 %"}, {0, 5});
        for (double x : {60.0, 160.0, 260.0}) addSwitch(QPointF(x, 172), {"ARRET", "10 %", "20 %", "30 %"}, {0, 10, 20, 30});
        terminals(cRed);
    }
    double g() const { return percent() / 100.0 / 36.3; }           // 4 kW sous 380 V etoile : 36,3 ohm par element a 100 %
    void elements(std::vector<BenchElem> &el) override {
        if (g() <= 0) return;
        for (int k = 0; k < 3; ++k) el.push_back({a1[k], a2[k], bcplx(g(), 0), g()});
    }
    void post(const BenchSolution &s) override {
        double u = 0, pw = 0;
        for (int k = 0; k < 3; ++k) {
            const double ua = std::abs(s.at(a1[k]) - s.at(a2[k])), ud = std::abs(s.dcAt(a1[k]) - s.dcAt(a2[k]));
            u = std::max({u, ua, ud});
            pw += (ua * ua + ud * ud) * g();
        }
        power = pw;
        warn = u > 245 ? QStringLiteral("SURTENSION %1 V par element : revoir le couplage").arg(fmt(u, 0)) : QString();
    }
    void paintExtra(QPainter *p) override {
        for (const Sw &q : sw) drawSwitch(p, q, (int)params[q.param].value, false);
        paintTerminals(p, QStringLiteral("4 kW  -  %1 %  -  %2 W").arg(fmt(percent(), 0)).arg(fmt(power, 0)));
    }
    QString plateHtml() const override {
        return couplingHtml(QStringLiteral("<h3>Charge resistive - 4 kW</h3>"
            "<p>4 kW TRI 380 V (etoile) &nbsp;|&nbsp; 4 kW TRI 220 V (triangle) &nbsp;|&nbsp; 4 kW MONO ou CONTINU 220 V<br>"
            "1,33 kW sous 127 V &nbsp;|&nbsp; 1 kW sous 110 V</p>"
            "<p>Commutateurs : 2 x (ARRET / 5 %) et 3 x (ARRET / 10 / 20 / 30 %). Element : 36,3 ohm a 100 %, 220 V max.</p>"),
            QStringLiteral("ou CONTINU"));
    }
    double power = 0;
};

class CapCart : public Cart {                        // charge capacitive 4 kvar
public:
    CapCart() : Cart(QStringLiteral("Charge capacitive 4 kVAR"), QColor(150, 140, 110)) {
        const double pc[6] = {5, 10, 10, 25, 25, 25};
        for (int i = 0; i < 6; ++i)
            addSwitch(QPointF(60 + 100 * (i % 3), i < 3 ? 78 : 172), {"ARRET", QStringLiteral("%1 %").arg(pc[i])}, {0, pc[i]});
        terminals(cRed);
    }
    double cap() const { return percent() / 100.0 * 87.7e-6; }      // 4 kvar sous 380 V etoile : 87,7 uF par element a 100 %
    void elements(std::vector<BenchElem> &el) override {
        if (cap() <= 0) return;
        for (int k = 0; k < 3; ++k) el.push_back({a1[k], a2[k], bcplx(0, PI2 * BenchWindow::kFreq * cap()), 0.0});
    }
    void post(const BenchSolution &s) override {
        double u = 0, q = 0;
        for (int k = 0; k < 3; ++k) {
            const double ua = std::abs(s.at(a1[k]) - s.at(a2[k]));
            u = std::max({u, ua, std::abs(s.dcAt(a1[k]) - s.dcAt(a2[k]))});
            q += ua * ua * PI2 * BenchWindow::kFreq * cap();
        }
        qvar = q;
        warn = u > 245 ? QStringLiteral("SURTENSION %1 V par element : revoir le couplage").arg(fmt(u, 0)) : QString();
    }
    void paintExtra(QPainter *p) override {
        for (const Sw &q : sw) drawSwitch(p, q, (int)params[q.param].value, true);
        paintTerminals(p, QStringLiteral("4 kVAR  -  %1 %  -  %2 var").arg(fmt(percent(), 0)).arg(fmt(qvar, 0)));
    }
    QString plateHtml() const override {
        return couplingHtml(QStringLiteral("<h3>Charge capacitive - 4 kVAR</h3>"
            "<p>4 kVA TRI 380 V (etoile) &nbsp;|&nbsp; 4 kVA TRI 220 V (triangle) &nbsp;|&nbsp; 4 kVA MONO 220 V<br>"
            "1,33 kVA sous 127 V &nbsp;|&nbsp; 1 kVA sous 110 V</p>"
            "<p>Commutateurs : 5 %, 10 %, 10 %, 25 %, 25 %, 25 %. Element : 87,7 uF a 100 %, 220 V max.</p>"), QString());
    }
    double qvar = 0;
};

// ---- Charge inductive a noyau plongeur : 6 bobinages (2 par phase), volant de reglage sur la face ------------
class IndCart : public BenchModule {
public:
    IndCart() : BenchModule(QStringLiteral("Charge inductive reglable"), 320, 450, QColor(150, 128, 92)) {
        setAcceptHoverEvents(true);
        plateOnly = true;
        for (int g = 0; g < 3; ++g)
            for (int r = 0; r < 2; ++r) {
                const int n = 2 * g + r + 1;
                const double x0 = 45 + 95 * g, y = 62 + 44 * r;
                e[n - 1] = addSocket(QStringLiteral("E%1").arg(n), QColor(110, 80, 50), x0, y);
                s[n - 1] = addSocket(QStringLiteral("S%1").arg(n), QColor(110, 80, 50), x0 + 42, y);
            }
        for (int k = 0; k < 6; ++k) spare[k] = addSocket(QStringLiteral("R%1").arg(k + 1), QColor(40, 150, 70), 110 + 50 * (k % 3), 300 + 38 * (k / 3));
        pe = addSocket("PE", cPE, 40, 372);
        mB = addSocket("N", cBlue, 40, 412);
        mR = addSocket("PH", cRed, 85, 412);
        for (int k = 0; k < 3; ++k) tri[k] = addSocket(QStringLiteral("L%1").arg(k + 1), cBlack, 190 + 45 * k, 412);
        params = { {"Position du volant (0 = L min, 100 = L max)", "%", 0, 100, 100, 1, 0} };
    }
    bool hasLinks() const override { return true; }
    void links(std::vector<std::pair<BenchSocket *, BenchSocket *>> &l) override {
        for (int k = 0; k < 3; ++k) l.push_back({tri[k], e[2 * k]});            // L1 = E1, L2 = E3, L3 = E5
        l.push_back({mR, e[0]});
        l.push_back({mB, s[1]});                                                  // N = S2
    }
    double L1() const { return 0.019 + (0.214 - 0.019) * params[0].value / 100.0; }   // un bobinage seul
    TrafoModel pair() const {                         // deux bobinages d'une meme colonne : couplage magnetique serre
        TrafoModel m{};
        const double wl = PI2 * BenchWindow::kFreq * L1(), r = 0.92;
        const bcplx z(r, wl), zm(0, 0.995 * wl), det = z * z - zm * zm;
        m.y11 = z / det; m.y22 = z / det; m.y12 = -zm / det;
        m.r1 = m.r2 = r;
        return m;
    }
    void elements(std::vector<BenchElem> &el) override {
        const TrafoModel m = pair();
        for (int g = 0; g < 3; ++g) m.stamp(e[2 * g], s[2 * g], e[2 * g + 1], s[2 * g + 1], el);
    }
    void post(const BenchSolution &sol) override {
        const TrafoModel m = pair();
        double im = 0;
        for (int g = 0; g < 3; ++g) {
            const bcplx v1 = sol.at(e[2 * g]) - sol.at(s[2 * g]), v2 = sol.at(e[2 * g + 1]) - sol.at(s[2 * g + 1]);
            im = std::max({im, std::abs(m.i1(v1, v2)), std::abs(m.i2(v1, v2)),
                           std::abs(sol.dcAt(e[2 * g]) - sol.dcAt(s[2 * g])) / 0.92, std::abs(sol.dcAt(e[2 * g + 1]) - sol.dcAt(s[2 * g + 1])) / 0.92});
        }
        imax = im;
        warn = im > 6.55 ? QStringLiteral("SURCHARGE %1 A par bobinage (6,5 A permanent)").arg(fmt(im, 1)) : QString();
    }
    QPointF dialC() const { return QPointF(160, 205); }
    void paintExtra(QPainter *p) override {
        const QPointF c = dialC();
        // volant de reglage
        p->setPen(QPen(QColor(15, 15, 15), 1));
        p->setBrush(QColor(225, 225, 220));
        p->drawEllipse(c, 30, 30);
        p->setPen(QPen(QColor(15, 15, 15), 9));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(c, 50, 50);
        p->save();
        p->translate(c);
        p->rotate(-150 + 3.0 * params[0].value);
        p->setPen(QPen(QColor(15, 15, 15), 6, Qt::SolidLine, Qt::RoundCap));
        for (int k = 0; k < 3; ++k) { p->drawLine(QPointF(0, 0), QPointF(0, -48)); p->rotate(120); }
        p->setPen(QPen(QColor(200, 202, 206), 5, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(0, -50), QPointF(0, -62));                        // poignee
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(25, 25, 27));
        p->drawEllipse(QPointF(0, 0), 14, 14);
        p->restore();
        QFont f = p->font(); f.setPixelSize(9); f.setBold(true); p->setFont(f);
        p->setPen(QColor(25, 25, 25));
        p->drawText(QRectF(10, 262, 300, 14), Qt::AlignCenter,
                    QStringLiteral("L = %1 H par bobinage (volant %2 %)  -  I max %3 A").arg(fmt(L1(), 3)).arg(fmt(params[0].value, 0)).arg(fmt(imax, 2)));
        f.setPixelSize(8); p->setFont(f);
        p->drawText(QRectF(10, 384, 110, 12), Qt::AlignCenter, QStringLiteral("MONOPHASE"));
        p->drawText(QRectF(170, 384, 130, 12), Qt::AlignCenter, QStringLiteral("TRIPHASE"));
        p->setPen(QPen(QColor(40, 40, 40), 2));
        p->setBrush(QColor(225, 70, 70));
        p->drawEllipse(QPointF(50, h - 2), 10, 10);
        p->drawEllipse(QPointF(w - 50, h - 2), 10, 10);
    }
    QString plateHtml() const override {
        return QStringLiteral(
            "<h3>Charge inductive reglable (noyau plongeur)</h3>"
            "<table border='1' cellspacing='0' cellpadding='4'><tr><th colspan='2'>PAR BOBINAGE (colonne de 2 enroulements)</th></tr>"
            "<tr><td><b>Montage PARALLELE</b></td><td>L max = 0,214 H &nbsp; L min = 0,019 H<br>I permanent = 13 A &nbsp; I max interm. = 21,7 A<br>R cuivre = 0,46 ohm</td></tr>"
            "<tr><td><b>Montage SERIE</b></td><td>L max = 0,856 H &nbsp; L min = 0,076 H<br>I permanent = 6,5 A &nbsp; I max interm. = 10,8 A<br>R cuivre = 1,84 ohm</td></tr></table>"
            "<p><b>Par colonne</b> (bornes E1 S1 / E2 S2, E3 S3 / E4 S4, E5 S5 / E6 S6)</p>"
            "<table border='1' cellspacing='0' cellpadding='4'>"
            "<tr><td>SERIE</td><td>barrettes S1 - E2, S3 - E4, S5 - E6 ; entrees E1 E3 E5, sorties S2 S4 S6</td></tr>"
            "<tr><td>PARALLELE</td><td>barrettes E1 - E2 et S1 - S2 (idem colonnes 2 et 3)</td></tr></table>"
            "<p><b>Entre colonnes</b></p>"
            "<table border='1' cellspacing='0' cellpadding='4'>"
            "<tr><td>ETOILE</td><td>S6 - S2 - S4 ensemble ; alimentation E1 E3 E5 (douilles TRIPHASE)</td></tr>"
            "<tr><td>TRIANGLE</td><td>S6 - E1, S2 - E3, S4 - E5 ; alimentation TRIPHASE</td></tr>"
            "<tr><td>MONO</td><td>S6 - S2 - S4 ensemble et E1 - E3 - E5 ensemble ; alimentation MONOPHASE (PH = E1, N = S2)</td></tr></table>"
            "<p>Volant : <b>%1 %</b> &nbsp; soit L = %2 H par enroulement (%3 H en serie).</p>")
            .arg(fmt(params[0].value, 0)).arg(fmt(L1(), 3)).arg(fmt(4 * L1(), 3));
    }
protected:
    bool onDial(QPointF p) const { return QLineF(p, dialC()).length() <= 66; }
    void turn(QPointF p) {
        const QPointF d = p - dialC();
        const double a = qBound(-150.0, qRadiansToDegrees(std::atan2(d.x(), -d.y())), 150.0);
        params[0].value = qRound((a + 150.0) / 3.0);
        update();
        if (changed) changed();
    }
    void mousePressEvent(QGraphicsSceneMouseEvent *ev) override {
        if (ev->button() == Qt::LeftButton && onDial(ev->pos())) { drag = true; turn(ev->pos()); ev->accept(); return; }
        BenchModule::mousePressEvent(ev);
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent *ev) override {
        if (drag) { turn(ev->pos()); ev->accept(); return; }
        BenchModule::mouseMoveEvent(ev);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *ev) override {
        if (drag) { drag = false; ev->accept(); return; }
        BenchModule::mouseReleaseEvent(ev);
    }
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *ev) override {
        if (onDial(ev->pos())) { ev->accept(); return; }
        BenchModule::mouseDoubleClickEvent(ev);
    }
    void wheelEvent(QGraphicsSceneWheelEvent *ev) override {
        if (!onDial(ev->pos())) { ev->ignore(); return; }
        const double step = (ev->modifiers() & Qt::ShiftModifier) ? 5 : 1;
        params[0].value = qBound(0.0, params[0].value + (ev->delta() > 0 ? step : -step), 100.0);
        update();
        if (changed) changed();
        ev->accept();
    }
    void hoverMoveEvent(QGraphicsSceneHoverEvent *ev) override {
        const bool d = onDial(ev->pos());
        setToolTip(d ? QStringLiteral("Volant : molette de la souris ou glisser pour regler l'inductance.")
                     : QStringLiteral("Double-clic : plaque signaletique et couplages."));
        setCursor(d ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
public:
    bool drag = false;
    double imax = 0;
    BenchSocket *e[6], *s[6], *spare[6], *pe, *mB, *mR, *tri[3];
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
             "Pince de courant (sortie oscilloscope)",
             "Charge resistive triphasee", "Charge inductive triphasee", "Charge capacitive triphasee",
             "Transformateur monophase", "Transformateur triphase" };
}

BenchModule *BenchWindow::addModule(const QString &kind, QPointF pos)
{
    BenchModule *m = nullptr;
    int i = kinds().indexOf(kind);
    if (i == 0)                                            // un seul pupitre par banc : c'est le meuble lui-meme
        for (BenchModule *o : m_modules)
            if (o->kind == 0) { if (m_label) m_label->setText(QStringLiteral("Le pupitre est deja sur le banc.")); return o; }
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
    case 17: m = new ResCart; break;
    case 18: m = new IndCart; break;
    case 19: m = new CapCart; break;
    case 20: m = new Trafo(false); break;
    case 21: m = new Trafo(true); break;
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
    // pupitre : place fixe ; appareils : sur la tablette de la console ; charges a gauche, transformateurs a droite
    if (m->kind == 0) return kBenchPos;
    const bool trafo = (m->kind == 20 || m->kind == 21);
    std::vector<QRectF> zones;
    if (m->instrument) zones = {kZoneTop, kZoneUnder};
    else if (trafo) zones = {kZoneRight, kZoneUnder, kZoneLeft};
    else zones = {kZoneLeft, kZoneUnder, kZoneRight};
    for (const QRectF &z : zones) {
        const bool onShelf = (z == kZoneTop);              // les appareils sont poses sur la tablette : on remplit par le bas
        const double span = z.height() - m->h;
        for (double k = 0; k <= span; k += 10) {
            const double y = onShelf ? z.bottom() - m->h - k : z.top() + k;
            for (double x = z.left(); x + m->w <= z.right(); x += 10) {
                const QRectF r(x, y, m->w, m->h);
                bool free = true;
                for (BenchModule *o : m_modules)
                    if (o != m && o->kind != 0 && o->sceneBoundingRect().adjusted(-14, -14, 14, 14).intersects(r)) { free = false; break; }
                if (free) return QPointF(x, y);
            }
        }
    }
    const QRectF &z = zones[0];                            // plus de place : on pose quand meme le module dans sa zone
    return QPointF(z.left() + 20 * (m_modules.size() % 10), z == kZoneTop ? z.bottom() - m->h : z.top());
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
    auto rect = [this](const QRectF &r, const QPen &pen, const QBrush &br, double z) {
        auto *it = m_scene->addRect(r, pen, br); it->setZValue(z); return it;
    };
    auto label = [this](const QString &t, double x, double y, const QColor &col) {
        auto *it = m_scene->addSimpleText(t); it->setPos(x, y); it->setBrush(col); it->setZValue(-12);
    };
    rect(QRectF(0, 0, kTableW, kFloorY), Qt::NoPen, QColor(236, 232, 214), -30);                     // mur
    rect(QRectF(0, kFloorY, kTableW, kTableH - kFloorY), Qt::NoPen, QColor(150, 170, 120), -30);     // sol
    // emplacements au sol, de part et d'autre du banc
    const QPen dash(QColor(150, 150, 140), 2, Qt::DashLine);
    rect(kZoneLeft.adjusted(-8, -8, 8, 8), dash, Qt::NoBrush, -25);
    rect(kZoneRight.adjusted(-8, -8, 8, 8), dash, Qt::NoBrush, -25);
    label(QStringLiteral("CHARGES (resistive, inductive, capacitive, rheostats...)"), kZoneLeft.left() + 4, kZoneLeft.top() - 28, QColor(120, 120, 110));
    label(QStringLiteral("TRANSFORMATEURS (monophase, triphase)"), kZoneRight.left() + 4, kZoneRight.top() - 28, QColor(120, 120, 110));
    label(QStringLiteral("APPAREILS DE MESURE (poses sur la tablette du pupitre)"), kZoneTop.left(), 4, QColor(120, 120, 110));
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
    if (!m_fitted) { m_fitted = true; m_view->fitInView(kViewBench, Qt::KeepAspectRatio); }
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
    m_scene->setSceneRect(-40, -40, kTableW + 80, kTableH + 80);
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
    edit->addAction(QStringLiteral("Vue : pupitre"), this, [this] { m_view->fitInView(kViewBench, Qt::KeepAspectRatio); });
    edit->addAction(QStringLiteral("Vue : console (gros plan)"), this, [this] {
        m_view->fitInView(QRectF(kBenchPos.x() + 40, kBenchPos.y() - 20, 2120, 360), Qt::KeepAspectRatio); });
    edit->addAction(QStringLiteral("Vue : banc entier"), this, [this] { m_view->fitInView(QRectF(0, 0, kTableW, kTableH), Qt::KeepAspectRatio); });
    QAction *zin = edit->addAction(QStringLiteral("Zoom +"));
    zin->setShortcut(QKeySequence::ZoomIn);
    connect(zin, &QAction::triggered, this, [this] { m_view->scale(1.2, 1.2); });
    QAction *zout = edit->addAction(QStringLiteral("Zoom -"));
    zout->setShortcut(QKeySequence::ZoomOut);
    connect(zout, &QAction::triggered, this, [this] { m_view->scale(1 / 1.2, 1 / 1.2); });
    menuBar()->addMenu(QStringLiteral("&Aide"))->addAction(QStringLiteral("Mode d'emploi"), this, [this] {
        m_label->setText(QStringLiteral("Glisser d'une douille a une autre = fil. Clic droit sur un fil = le retirer. "
                                        "Double-clic sur un module = reglages. Molette = reglage principal. "
                                        "Pupitre : cliquer sur le sectionneur, les boutons et les commutateurs ; "
                                        "tourner les molettes a la molette de la souris ou en les faisant glisser."));
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
