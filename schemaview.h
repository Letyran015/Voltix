#pragma once
#include "project.h"

#include <QByteArray>
#include <QGraphicsView>
#include <QList>
#include <QMap>
#include <QPolygonF>
#include <QString>
#include <functional>

class WireItem;
class SymbolItem;
class SheetItem;

class SchemaView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit SchemaView(QWidget *parent = nullptr);

    void setSheetEnabled(bool on);
    bool sheetEnabled() const { return m_sheetOn; }

    void setKind(const QString &kind) { m_kind = kind; }
    void setWireNature(const QString &id) { m_wireNature = id; }
    void setWireSection(const QString &s) { m_wireSection = s; }

    void clearAll();                              // nouveau projet
    bool saveToFile(const QString &path);
    bool loadFromFile(const QString &path);
    bool exportPdf(const QString &path);

    bool isModified() const { return m_dirty; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    void undo();
    void redo();

    // projet, feuille et folios
    ProjectInfo project() const { return m_pi; }
    void setProject(const ProjectInfo &pi);
    int folioCount() const { return int(m_folios.size()); }
    int currentFolio() const { return m_folio; }
    QString folioTitle(int i) const;
    void setFolio(int i);
    void addFolio();
    bool removeFolio();
    void renameFolio(int i, const QString &title);
    void zoomFit();

    void simulateDC();
    void simulateTran();
    void simulateSweep();
    void simulateAC();
    void verifyElectrical();

signals:
    void stateChanged();      // historique ou état « modifié » changé
    void foliosChanged();     // liste ou titres des folios, folio courant

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
    bool m_sheetOn = true;
    void wireClick(const QPointF &scenePos);
    void finishWire(bool keepLast = false);
    void cancelWire();
    void setWireMode(bool on);
    bool pinAt(const QPointF &p, SymbolItem *&sym, int &pin) const;
    bool onWire(const QPointF &p) const;
    void dumpNets();
    void drawJunctions(QPainter *p) const;

    void collect(QList<SymbolItem *> &syms, QList<WireItem *> &wires) const;
    QByteArray serialize() const;
    bool deserialize(const QByteArray &data, bool recenter);
    static QByteArray emptyState();
    static bool validState(const QByteArray &data);
    void resetScene();
    void resetCounters();
    void scanCounters(const QByteArray &data);
    void commit();

    void ensureSheet();
    void applySheet(int index);
    void storeCurrent();
    void loadFolio(int i);
    void forEachFolio(const std::function<void(int)> &fn);

    bool m_panning = false;
    QPoint m_lastPos;

    bool m_wireMode = false;
    WireItem *m_current = nullptr;
    QPolygonF m_pts;

    QString m_kind = "resistor";
    QString m_wireNature = "OTHER", m_wireSection;
    int m_total = 0;
    QMap<QString, int> m_counts;

    ProjectInfo m_pi;
    QList<Folio> m_folios;
    int m_folio = 0;
    SheetItem *m_sheet = nullptr;
    bool m_dirty = false;
    bool m_exporting = false;

    QList<QByteArray> m_undo, m_redo;
    QByteArray m_currentState;
};