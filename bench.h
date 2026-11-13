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
#include <utility>

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
    int idx = -1;                    // rang dans BenchSolution::all
    // sortie "active" (sonde, pince) : tension fournie a l'oscilloscope
    std::function<bcplx(const BenchSolution &)> source;
    std::function<double(const BenchSolution &)> sourceDc;
    std::function<double()> scale;   // grandeur reelle par volt a l'oscilloscope (rapport de la sonde)
    QString unit = QStringLiteral("V");   // unite affichee par l'oscilloscope pour cette sonde
    std::vector<BenchWire *> wires;
};

// ---- Fil banane ---------------------------------------------------------------
class BenchWire : public QGraphicsPathItem {
public:
    BenchWire(BenchSocket *a, BenchSocket *b, const QColor &col);
    ~BenchWire() override;
    void updatePath();
    QColor color() const { return pen().color(); }
    QPainterPath shape() const override;
    QRectF boundingRect() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w) override;
    void refreshClamp() { prepareGeometryChange(); clamped = !clamps.empty(); update(); }
    BenchSocket *a, *b;
    std::vector<BenchModule *> clamps;   // pinces posees sur ce fil
    bool clamped = false;
};

// ---- Elements du calcul ----------------------------------------------------------
struct BenchElem { BenchSocket *a, *b; bcplx y; double ydc = -1; };   // ydc < 0 : meme valeur reelle qu'en alternatif          // admittance entre deux douilles
struct BenchSrc  { BenchSocket *p, *n; bcplx v; double vdc = 0.0; };          // source de tension ideale
struct BenchSolution {
    std::vector<bcplx> v;                              // potentiel de chaque reseau (0 = masse)
    std::vector<BenchSocket *> all;                    // toutes les douilles du banc
    std::vector<double> vdc;                           // potentiel continu de chaque reseau
    double dcAt(const BenchSocket *s) const {
        return (s->net >= 0 && s->net < (int)vdc.size()) ? vdc[s->net] : 0.0;
    }
    std::function<double(const BenchSocket *, const BenchSocket *)> rdc;   // resistance continue entre 2 douilles (sources court-circuitees)
    std::vector<bcplx> iin;                            // courant entrant dans chaque douille (depuis le fil)
    std::vector<double> iindc;
    bcplx iinAt(const BenchSocket *s) const {
        return (s->idx >= 0 && s->idx < (int)iin.size()) ? iin[s->idx] : bcplx(0, 0);
    }
    double iinDcAt(const BenchSocket *s) const {
        return (s->idx >= 0 && s->idx < (int)iindc.size()) ? iindc[s->idx] : 0.0;
    }
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
    QPainterPath shape() const override { QPainterPath b = bodyPath(); if (b.isEmpty()) { b.addRect(boundingRect()); } return b; }
    virtual QPainterPath bodyPath() const { return QPainterPath(); }   // silhouette propre (sinon rectangle arrondi)
    virtual double titleY() const { return 4; }
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;

    BenchSocket *addSocket(const QString &name, const QColor &col, double x, double y);

    virtual void elements(std::vector<BenchElem> &) {}
    virtual void sources(std::vector<BenchSrc> &) {}
    virtual void links(std::vector<std::pair<BenchSocket *, BenchSocket *>> &) {}   // douilles reliees en interne
    virtual void post(const BenchSolution &) {}
    virtual void paintExtra(QPainter *) {}
    virtual bool isSupply() const { return false; }
    virtual bool hasLinks() const { return false; }
    virtual bool canClamp() const { return false; }
    virtual BenchWire *clampedWire() const { return nullptr; }
    virtual void clampTo(BenchWire *) {}
    virtual void onWireRemoved(BenchWire *) {}

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

// ---- Anneau de pince (machoire) : un clic puis un clic sur un fil ----------------------------------
class BenchJaw : public QGraphicsEllipseItem {
public:
    BenchJaw(BenchModule *m, QPointF pos, double r);
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
    BenchModule *module;
};

// ---- Scene (creation des fils a la souris) ------------------------------------------
class BenchScene : public QGraphicsScene {
public:
    std::function<void(BenchSocket *, BenchSocket *)> onConnect;
    std::function<void()> onDeleteSelection;
    std::function<void(BenchWire *)> onDeleteWire;
    std::function<void()> onClampChanged;
    std::function<void(const QString &)> onHint;
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
private:
    BenchSocket *socketAt(QPointF p) const;
    BenchSocket *from = nullptr;
    BenchModule *pendingClamp = nullptr;
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
    bool saveToFile(const QString &path) const;
    bool loadFromFile(const QString &path);
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
