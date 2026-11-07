#include "schemaview.h"
#include "symbolitem.h"
#include "wireitem.h"

#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QtMath>

static const int GRID = 20;

static QPointF snap(const QPointF &p)
{
    return QPointF(qRound(p.x() / GRID) * GRID, qRound(p.y() / GRID) * GRID);
}

// force un segment horizontal ou vertical
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
    const qreal left = qFloor(rect.left() / GRID) * GRID;
    const qreal top  = qFloor(rect.top()  / GRID) * GRID;
    for (qreal x = left; x < rect.right(); x += GRID)
        for (qreal y = top; y < rect.bottom(); y += GRID)
            p->drawPoint(QPointF(x, y));
}

void SchemaView::wheelEvent(QWheelEvent *e)
{
    const qreal factor = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const qreal next = transform().m11() * factor;
    if (next < 0.1 || next > 10.0)
        return;
    scale(factor, factor);
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
    const QPointF end = ortho(last, p);
    if (end == last)
        return;
    m_pts.last() = end;                     // fige le segment
    m_pts << end;                           // nouveau segment d'aperçu
    m_current->setPoints(m_pts);
}

void SchemaView::finishWire()
{
    if (!m_current)
        return;
    m_pts.removeLast();                     // retire le segment d'aperçu
    if (m_pts.size() < 2) {
        scene()->removeItem(m_current);
        delete m_current;
    } else {
        m_current->setPoints(m_pts);
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
    if (m_wireMode && m_current) {          // aperçu du segment en cours
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
    if (m_wireMode) {                       // un double-clic rapide compte comme un clic
        if (e->button() == Qt::LeftButton)
            wireClick(mapToScene(pos));
        return;
    }
    if (e->button() == Qt::LeftButton && !itemAt(pos)) {
        auto *r = new ResistorItem;
        r->setPos(snap(mapToScene(pos)));
        scene()->addItem(r);
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(e);
}

void SchemaView::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_W) {                        // outil fil : on/off
        setWireMode(!m_wireMode);
    } else if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return) {
        finishWire();                                   // termine le fil en cours
    } else if (e->key() == Qt::Key_R) {                 // rotation de 90°
        for (QGraphicsItem *it : scene()->selectedItems())
            it->setRotation(it->rotation() + 90);
    } else if (e->key() == Qt::Key_Delete) {            // suppression
        const auto items = scene()->selectedItems();
        for (QGraphicsItem *it : items) {
            scene()->removeItem(it);
            delete it;
        }
    } else {
        QGraphicsView::keyPressEvent(e);
    }
}