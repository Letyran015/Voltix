// Voltix - banc virtuel d'electrotechnique (palier 1)
// Fenetre independante : modules a douilles + fils bananes + calcul en regime
// sinusoidal permanent (50 Hz). Aucun lien avec le schema ni avec ngspice.
#pragma once
#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QColor>
#include <functional>
#include <complex>
#include <vector>

using bcplx = std::complex<double>;

class BenchModule;
class BenchWire;
struct BenchSolution;
class QLabel;
class QShowEvent;
class QWheelEvent;
class QComboBox;

// ---- Douille ---------------------------------------------------------------
class BenchSocket : public QGraphicsEllipseItem {
public:
    BenchSocket(BenchModule *m, const QString &name, const QColor &col, QPointF pos);
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
    QPointF center() const { return mapToScene(QPointF(0, 0)); }
    BenchModule *module;
    QString name;
    QColor col;
    int net = -1;                    // numero de reseau apres calcul
    // sortie "active" (sonde, pince) : tension fournie a l'oscilloscope
    std::function<bcplx(const BenchSolution &)> source;
    std::vector<BenchWire *> wires;
};

// ---- Fil banane ---------------------------------------------------------------
class BenchWire : public QGraphicsPathItem {
public:
    BenchWire(BenchSocket *a, BenchSocket *b, const QColor &col);
    ~BenchWire() override;
    void updatePath();
    BenchSocket *a, *b;
};

// ---- Elements du calcul ----------------------------------------------------------
struct BenchElem { BenchSocket *a, *b; bcplx y; };          // admittance entre deux douilles
struct BenchSrc  { BenchSocket *p, *n; bcplx v; };          // source de tension ideale
struct BenchSolution {
    std::vector<bcplx> v;                              // potentiel de chaque reseau (0 = masse)
    std::vector<BenchSocket *> all;                    // toutes les douilles du banc
    bcplx at(const BenchSocket *s) const {
        return (s->net >= 0 && s->net < (int)v.size()) ? v[s->net] : bcplx(0, 0);
    }
};

struct BenchParam {
    QString name, unit;
    double min, max, value, step;
    int decimals = 2;
};

// ---- BenchModule (face avant d'un appareil) ----------------------------------------------
class BenchModule : public QGraphicsItem {
public:
    BenchModule(const QString &title, int w, int h, const QColor &body);
    QRectF boundingRect() const override { return QRectF(0, 0, w, h); }
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;

    BenchSocket *addSocket(const QString &name, const QColor &col, double x, double y);

    virtual void elements(std::vector<BenchElem> &) {}
    virtual void sources(std::vector<BenchSrc> &) {}
    virtual void post(const BenchSolution &) {}
    virtual void paintExtra(QPainter *) {}
    virtual bool isSupply() const { return false; }

    QString title;
    int w, h;
    QColor body;
    std::vector<BenchSocket *> sockets;
    std::vector<BenchParam> params;
    int wheelParam = -1;
    bool instrument = false;     // pupitre / appareils : rail du haut ; sinon plan de travail
    int kind = -1;               // rang dans BenchWindow::kinds()
    QString warn;
    std::function<void()> changed;

protected:
    QVariant itemChange(GraphicsItemChange c, const QVariant &v) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override;
    void wheelEvent(QGraphicsSceneWheelEvent *e) override;
};

// ---- Scene (creation des fils a la souris) ------------------------------------------
class BenchScene : public QGraphicsScene {
public:
    std::function<void(BenchSocket *, BenchSocket *)> onConnect;
    std::function<void()> onDeleteSelection;
    std::function<void(BenchWire *)> onDeleteWire;
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
private:
    BenchSocket *socketAt(QPointF p) const;
    BenchSocket *from = nullptr;
    QGraphicsLineItem *temp = nullptr;
};

// ---- Vue (Ctrl + molette = zoom) --------------------------------------------------------
class BenchView : public QGraphicsView {
public:
    using QGraphicsView::QGraphicsView;
protected:
    void wheelEvent(QWheelEvent *e) override;
};

// ---- Fenetre du banc ----------------------------------------------------------------
class BenchWindow : public QMainWindow {
public:
    explicit BenchWindow(QWidget *parent = nullptr);

    BenchModule *addModule(const QString &kind, QPointF pos = QPointF(-1e9, 0));
    bool connectSockets(BenchSocket *a, BenchSocket *b, const QColor &col);
    void removeWire(BenchWire *w);
    void removeModule(BenchModule *m);
    void clearAll();
    void arrange();                    // range les modules sur la table
    bool recompute();                  // false si circuit non resoluble
    QString statusText() const { return m_status; }
    const std::vector<BenchModule *> &modules() const { return m_modules; }

    static QStringList kinds();
    static constexpr double kFreq = 50.0;

protected:
    void showEvent(QShowEvent *e) override;

private:
    QPointF findSlot(BenchModule *m) const;
    void drawTable();
    bool m_fitted = false;
    BenchScene *m_scene;
    BenchView *m_view;
    QLabel *m_label;
    QComboBox *m_wireColor;
    std::vector<BenchModule *> m_modules;
    std::vector<BenchWire *> m_wires;
    QString m_status;
};
