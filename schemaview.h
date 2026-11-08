#pragma once
#include <QGraphicsView>
#include <QPolygonF>
#include <QMap>
#include <QString>

class WireItem;
class SymbolItem;

class SchemaView : public QGraphicsView
{
public:
    explicit SchemaView(QWidget *parent = nullptr);
        void setKind(const QString &kind) { m_kind = kind; }
        void clearAll();
        bool saveToFile(const QString &path);
        bool loadFromFile(const QString &path);

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

    QString m_kind = "resistor";
    int m_total = 0;
    QMap<QString, int> m_counts;
};