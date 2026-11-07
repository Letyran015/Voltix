#pragma once
#include <QGraphicsView>
#include <QPolygonF>

class WireItem;
class SymbolItem;

class SchemaView : public QGraphicsView
{
public:
    explicit SchemaView(QWidget *parent = nullptr);

protected:
    void drawBackground(QPainter *p, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    void wireClick(const QPointF &scenePos);
    void finishWire(bool keepLast = false);
    void setWireMode(bool on);
    bool pinAt(const QPointF &p, SymbolItem *&sym, int &pin) const;
    void dumpNets();

    bool m_panning = false;
    QPoint m_lastPos;

    bool m_wireMode = false;
    WireItem *m_current = nullptr;
    QPolygonF m_pts;

    int m_resCount = 0;
};