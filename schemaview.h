#pragma once
#include <QGraphicsView>

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
    bool m_panning = false;
    QPoint m_lastPos;
};