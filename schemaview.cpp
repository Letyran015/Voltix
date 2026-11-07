#include "schemaview.h"
#include "symbolitem.h"
#include "wireitem.h"
#include "core.h"

#include <QDebug>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMap>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <QMessageBox>

static const int GRID = 20;

static QPointF snap(const QPointF &p)
{
    return QPointF(qRound(p.x() / GRID) * GRID, qRound(p.y() / GRID) * GRID);
}

static QPointF ortho(const QPointF &from, const QPointF &to)
{
    if (qAbs(to.x() - from.x()) >= qAbs(to.y() - from.y()))
        return QPointF(to.x(), from.y());
    return QPointF(from.x(), to.y());
}

SchemaView::SchemaView(QWidget *parent) : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    scene()->setSceneRect(-5000, -5000, 10000, 10000);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(RubberBandDrag);
    setViewportUpdateMode(FullViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void SchemaView::drawBackground(QPainter *p, const QRectF &rect)
{
    p->fillRect(rect, Qt::white);
    if (transform().m11() < 0.3)
        return;

    p->setPen(QPen(QColor(170, 170, 170), 0));
    const int x0 = qFloor(rect.left() / GRID);
    const int x1 = qCeil(rect.right() / GRID);
    const int y0 = qFloor(rect.top() / GRID);
    const int y1 = qCeil(rect.bottom() / GRID);
    for (int ix = x0; ix <= x1; ++ix)
        for (int iy = y0; iy <= y1; ++iy)
            p->drawPoint(QPointF(ix * GRID, iy * GRID));
}

void SchemaView::wheelEvent(QWheelEvent *e)
{
    const qreal factor = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const qreal next = transform().m11() * factor;
    if (next < 0.1 || next > 10.0)
        return;
    scale(factor, factor);
}

bool SchemaView::pinAt(const QPointF &p, SymbolItem *&sym, int &pin) const
{
    for (QGraphicsItem *it : scene()->items()) {
        auto *s = dynamic_cast<SymbolItem *>(it);
        if (!s) continue;
        for (int i = 0; i < s->pinCount(); ++i) {
            if (QLineF(s->pinScene(i), p).length() < 4) {
                sym = s;
                pin = i;
                return true;
            }
        }
    }
    return false;
}

void SchemaView::setWireMode(bool on)
{
    finishWire();
    m_wireMode = on;
    setDragMode(on ? NoDrag : RubberBandDrag);
    viewport()->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

void SchemaView::wireClick(const QPointF &scenePos)
{
    const QPointF p = snap(scenePos);

    if (!m_current) {                       // premier clic : début du fil
        m_pts.clear();
        m_pts << p << p;
        m_current = new WireItem;
        m_current->setPoints(m_pts);
        scene()->addItem(m_current);
        return;
    }

    const QPointF last = m_pts[m_pts.size() - 2];

    SymbolItem *s = nullptr;
    int pin = -1;
    if (pinAt(p, s, pin)) {                 // clic sur une broche : on termine le fil
        if (p == last)
            return;
        if (!qFuzzyCompare(p.x() + 1, last.x() + 1) && !qFuzzyCompare(p.y() + 1, last.y() + 1)) {
            m_pts.last() = QPointF(p.x(), last.y());   // coude
            m_pts << p;
        } else {
            m_pts.last() = p;
        }
        m_current->setPoints(m_pts);
        finishWire(true);
        return;
    }

    const QPointF end = ortho(last, p);
    if (end == last)
        return;
    m_pts.last() = end;                     // fige le segment
    m_pts << end;                           // nouveau segment d'aperçu
    m_current->setPoints(m_pts);
}

void SchemaView::finishWire(bool keepLast)
{
    if (!m_current)
        return;
    if (!keepLast)
        m_pts.removeLast();                 // retire le segment d'aperçu

    if (m_pts.size() < 2) {
        delete m_current;
    } else {
        SymbolItem *sa = nullptr, *sb = nullptr;
        int pa = -1, pb = -1;
        pinAt(m_pts.first(), sa, pa);       // accroche aux broches trouvées aux extrémités
        pinAt(m_pts.last(), sb, pb);
        m_current->setPoints(m_pts);
        m_current->setEnds(sa, pa, sb, pb);
    }
    m_current = nullptr;
    m_pts.clear();
}

void SchemaView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPos = e->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (m_wireMode) {
        if (e->button() == Qt::LeftButton)
            wireClick(mapToScene(e->position().toPoint()));
        else if (e->button() == Qt::RightButton)
            finishWire();
        return;
    }
    QGraphicsView::mousePressEvent(e);
}

void SchemaView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_panning) {
        const QPoint pos = e->position().toPoint();
        const QPoint d = pos - m_lastPos;
        m_lastPos = pos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        return;
    }
    if (m_wireMode && m_current) {
        const QPointF p = snap(mapToScene(e->position().toPoint()));
        m_pts.last() = ortho(m_pts[m_pts.size() - 2], p);
        m_current->setPoints(m_pts);
        return;
    }
    QGraphicsView::mouseMoveEvent(e);
}

void SchemaView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        viewport()->setCursor(m_wireMode ? Qt::CrossCursor : Qt::ArrowCursor);
        return;
    }
    if (m_wireMode)
        return;
    QGraphicsView::mouseReleaseEvent(e);
}

void SchemaView::mouseDoubleClickEvent(QMouseEvent *e)
{
    const QPoint pos = e->position().toPoint();
    if (m_wireMode) {
        if (e->button() == Qt::LeftButton)
            wireClick(mapToScene(pos));
        return;
    }
    if (e->button() == Qt::LeftButton && !itemAt(pos)) {
        SymbolItem *s = makeSymbol(m_kind);
        if (s) {
            s->index = ++m_total;
            s->ref = s->prefix() + QString::number(++m_counts[s->prefix()]);
            s->setPos(snap(mapToScene(pos)));
            scene()->addItem(s);
        }
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(e);
}

void SchemaView::dumpNets()
{
    QList<SymbolItem *> syms;
    QList<WireItem *> wires;
    for (QGraphicsItem *it : scene()->items()) {
        if (auto *s = dynamic_cast<SymbolItem *>(it)) syms << s;
        else if (auto *w = dynamic_cast<WireItem *>(it)) wires << w;
    }
    std::sort(syms.begin(), syms.end(),
              [](SymbolItem *a, SymbolItem *b) { return a->index < b->index; });

    // 1. on construit le modèle C à partir de la scène
    Schema *sc = schema_new();
    for (int c = 0; c < syms.size(); ++c) {
        for (int i = 0; i < syms[c]->pinCount(); ++i) {
            const QPointF p = syms[c]->pinScene(i);
            schema_add_pin(sc, c, i, qRound(p.x()), qRound(p.y()));
        }
    }
    for (WireItem *w : std::as_const(wires)) {
        const QPolygonF &pts = w->points();
        if (pts.size() < 2) continue;
        schema_add_wire(sc, qRound(pts.first().x()), qRound(pts.first().y()),
                        qRound(pts.last().x()),  qRound(pts.last().y()));
    }

    // 2. le noyau calcule les nets
    schema_compute_nets(sc);

    // 3. on affiche le résultat
    QMap<int, QStringList> nets;
    QStringList floating;
    for (int c = 0; c < syms.size(); ++c) {
        for (int i = 0; i < syms[c]->pinCount(); ++i) {
            const QString name = QString("%1.%2").arg(syms[c]->ref).arg(i + 1);
            const int n = schema_pin_net(sc, c, i);
            if (n >= 0) nets[n] << name;
            else        floating << name;
        }
    }
    QString text = QString("%1 net(s)\n\n").arg(nets.size());
    for (auto it = nets.cbegin(); it != nets.cend(); ++it) {
        const bool gnd = !it.value().filter("GND").isEmpty();
        text += QString("Net %1%2 : %3\n")
                    .arg(it.key())
                    .arg(gnd ? " (masse)" : "")
                    .arg(it.value().join(", "));
    }
    if (!floating.isEmpty())
        text += QString("\nNon connectées : %1").arg(floating.join(", "));

    qDebug().noquote() << text;
    QMessageBox::information(this, "Netlist", text);
    schema_free(sc);
}

void SchemaView::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_W) {
        setWireMode(!m_wireMode);
    } else if (e->key() == Qt::Key_N) {
        dumpNets();
    } else if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return) {
        finishWire();
    } else if (e->key() == Qt::Key_R) {
        for (QGraphicsItem *it : scene()->selectedItems()) {
            it->setRotation(it->rotation() + 90);
            if (auto *s = dynamic_cast<SymbolItem *>(it))
                s->updateWires();
        }
    } else if (e->key() == Qt::Key_Delete) {
        const auto items = scene()->selectedItems();
        for (QGraphicsItem *it : items)
            delete it;
    } else {
        QGraphicsView::keyPressEvent(e);
    }
}