#include "schemaview.h"
#include "symbolitem.h"

#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QtMath>

static const int GRID = 20;

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
    if (transform().m11() < 0.3)   // trop dézoomé : on n'affiche pas la grille
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

void SchemaView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPos = e->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
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
    QGraphicsView::mouseMoveEvent(e);
}

void SchemaView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        unsetCursor();
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
}

void SchemaView::mouseDoubleClickEvent(QMouseEvent *e)
{
    const QPoint pos = e->position().toPoint();
    if (e->button() == Qt::LeftButton && !itemAt(pos)) {
        const QPointF sp = mapToScene(pos);
        auto *r = new ResistorItem;
        r->setPos(qRound(sp.x() / GRID) * GRID, qRound(sp.y() / GRID) * GRID);
        scene()->addItem(r);
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(e);
}

void SchemaView::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_R) {                       // rotation de 90°
        for (QGraphicsItem *it : scene()->selectedItems())
            it->setRotation(it->rotation() + 90);
    } else if (e->key() == Qt::Key_Delete) {           // suppression
        const auto items = scene()->selectedItems();
        for (QGraphicsItem *it : items) {
            scene()->removeItem(it);
            delete it;
        }
    } else {
        QGraphicsView::keyPressEvent(e);
    }
}