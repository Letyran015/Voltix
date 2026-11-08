#pragma once
#include <QByteArray>
#include <QGraphicsView>
#include <QList>
#include <QMap>
#include <QPolygonF>
#include <QString>

class WireItem;
class SymbolItem;

class SchemaView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit SchemaView(QWidget *parent = nullptr);

    void setKind(const QString &kind) { m_kind = kind; }

    void clearAll();                              // vide le schéma et l'historique
    bool saveToFile(const QString &path);
    bool loadFromFile(const QString &path);

    bool isModified() const { return m_current != m_saved; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    void undo();
    void redo();

    void simulateDC();        // point de fonctionnement
    void simulateTran();      // transitoire
    void simulateSweep();     // balayage DC
    void simulateAC();        // analyse AC (Bode)

signals:
    void stateChanged();      // historique ou état « modifié » changé

protected:
    void drawBackground(QPainter *p, const QRectF &rect) override;
    void drawForeground(QPainter *p, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    void wireClick(const QPointF &scenePos);
    void finishWire(bool keepLast = false);
    void cancelWire();
    void setWireMode(bool on);
    bool pinAt(const QPointF &p, SymbolItem *&sym, int &pin) const;
    bool onWire(const QPointF &p) const;
    void dumpNets();

    void collect(QList<SymbolItem *> &syms, QList<WireItem *> &wires) const;
    QByteArray serialize() const;
    bool deserialize(const QByteArray &data, bool recenter);
    void resetScene();
    void commit();             // enregistre l'état courant dans l'historique s'il a changé

    bool m_panning = false;
    QPoint m_lastPos;

    bool m_wireMode = false;
    WireItem *m_current = nullptr;
    QPolygonF m_pts;

    QString m_kind = "resistor";
    int m_total = 0;
    QMap<QString, int> m_counts;

    QList<QByteArray> m_undo, m_redo;
    QByteArray m_current_state_placeholder_unused;
    QByteArray m_currentState, m_saved;
};