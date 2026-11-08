#include "schemaview.h"
#include "symbolitem.h"
#include "wireitem.h"
#include "propertiesdialog.h"
#include "plotwidget.h"
#include "spice.h"
#include "core.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <QDir>
#include <QListWidget>

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

// ------------------------------------------------------------------
//  Construction, fond et zoom
// ------------------------------------------------------------------
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

    m_currentState = serialize();
    m_saved = m_currentState;
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

// points de jonction : extrémité d'un fil posée sur un autre fil, ou 3 extrémités au même endroit
void SchemaView::drawForeground(QPainter *p, const QRectF &)
{
    struct End { QPointF pt; WireItem *w; };
    QList<WireItem *> wires;
    QList<End> ends;
    const auto items = scene()->items();
    for (QGraphicsItem *it : items)
        if (auto *w = dynamic_cast<WireItem *>(it)) {
            wires << w;
            if (w->points().size() >= 2) {
                ends << End{w->points().first(), w};
                ends << End{w->points().last(), w};
            }
        }

    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 110, 0));
    QList<QPointF> done;
    for (const End &e : std::as_const(ends)) {
        bool junction = false;
        for (WireItem *w : std::as_const(wires))
            if (w != e.w && w->touches(e.pt)) { junction = true; break; }
        if (!junction) {
            int same = 0;
            for (const End &o : std::as_const(ends))
                if (QLineF(o.pt, e.pt).length() < 1) ++same;
            junction = same >= 3;
        }
        if (!junction) continue;
        bool already = false;
        for (const QPointF &d : std::as_const(done))
            if (QLineF(d, e.pt).length() < 1) { already = true; break; }
        if (already) continue;
        done << e.pt;
        p->drawEllipse(e.pt, 4.5, 4.5);
    }
}

void SchemaView::wheelEvent(QWheelEvent *e)
{
    const qreal factor = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const qreal next = transform().m11() * factor;
    if (next < 0.1 || next > 10.0)
        return;
    scale(factor, factor);
}

// ------------------------------------------------------------------
//  Historique (annuler / rétablir) et sérialisation
// ------------------------------------------------------------------
void SchemaView::collect(QList<SymbolItem *> &syms, QList<WireItem *> &wires) const
{
    const auto items = scene()->items();
    for (QGraphicsItem *it : items) {
        if (auto *s = dynamic_cast<SymbolItem *>(it))
            syms << s;
        else if (auto *w = dynamic_cast<WireItem *>(it)) {
            if (w != m_current)          // on ignore le fil en cours de tracé
                wires << w;
        }
    }
    std::sort(syms.begin(), syms.end(),
              [](SymbolItem *a, SymbolItem *b) { return a->index < b->index; });
}

QByteArray SchemaView::serialize() const
{
    QList<SymbolItem *> syms;
    QList<WireItem *> wires;
    collect(syms, wires);

    QHash<SymbolItem *, int> position;
    QJsonArray comps;
    for (int i = 0; i < syms.size(); ++i) {
        SymbolItem *s = syms[i];
        position[s] = i;
        QJsonObject o;
        o["kind"] = s->kind();
        o["ref"] = s->ref;
        o["index"] = s->index;
        o["x"] = s->pos().x();
        o["y"] = s->pos().y();
        o["rotation"] = s->rotation();
        o["showRef"] = s->showRef;
        QJsonArray pa;
        for (const Prop &pr : std::as_const(s->props)) {
            QJsonObject po;
            po["name"] = pr.name;
            po["value"] = pr.value;
            po["show"] = pr.show;
            pa.append(po);
        }
        o["props"] = pa;
        comps.append(o);
    }

    auto endToJson = [&](SymbolItem *s, int pin) {
        QJsonObject e;
        e["comp"] = position.value(s, -1);
        e["pin"] = pin;
        return e;
    };

    QJsonArray ws;
    for (WireItem *w : std::as_const(wires)) {
        QJsonArray pts;
        for (const QPointF &p : w->points()) {
            QJsonArray xy;
            xy.append(p.x());
            xy.append(p.y());
            pts.append(xy);
        }
        QJsonObject o;
        o["points"] = pts;
        if (w->symA()) o["a"] = endToJson(w->symA(), w->pinA());
        if (w->symB()) o["b"] = endToJson(w->symB(), w->pinB());
        ws.append(o);
    }

    QJsonObject root;
    root["format"] = "voltix";
    root["version"] = 1;
    root["components"] = comps;
    root["wires"] = ws;
    return QJsonDocument(root).toJson();
}

void SchemaView::resetScene()
{
    cancelWire();
    scene()->clear();
    m_total = 0;
    m_counts.clear();
}

bool SchemaView::deserialize(const QByteArray &data, bool recenter)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    if (root["format"].toString() != "voltix")
        return false;

    resetScene();                           // le contenu est valide : on peut vider la scène

    QList<SymbolItem *> syms;
    const QJsonArray comps = root["components"].toArray();
    for (const QJsonValue &v : comps) {
        const QJsonObject o = v.toObject();
        SymbolItem *s = makeSymbol(o["kind"].toString());
        if (!s) {
            syms << nullptr;
            continue;
        }
        s->ref = o["ref"].toString();
        s->index = o["index"].toInt();
        s->showRef = o["showRef"].toBool(s->showRef);
        const QJsonArray pa = o["props"].toArray();
        for (const QJsonValue &pv : pa) {
            const QJsonObject po = pv.toObject();
            for (Prop &pr : s->props)
                if (pr.name == po["name"].toString()) {
                    pr.value = po["value"].toString();
                    pr.show = po["show"].toBool();
                }
        }
        s->setRotation(o["rotation"].toDouble());
        s->setPos(o["x"].toDouble(), o["y"].toDouble());
        scene()->addItem(s);
        syms << s;

        m_total = qMax(m_total, s->index);
        const int n = s->ref.mid(s->prefix().size()).toInt();
        m_counts[s->prefix()] = qMax(m_counts.value(s->prefix()), n);
    }

    auto endOf = [&](const QJsonValue &v, SymbolItem *&s, int &pin) {
        s = nullptr;
        pin = -1;
        if (!v.isObject())
            return;
        const QJsonObject e = v.toObject();
        const int c = e["comp"].toInt(-1);
        if (c < 0 || c >= syms.size() || !syms[c])
            return;
        const int p = e["pin"].toInt(-1);
        if (p < 0 || p >= syms[c]->pinCount())
            return;
        s = syms[c];
        pin = p;
    };

    const QJsonArray ws = root["wires"].toArray();
    for (const QJsonValue &v : ws) {
        const QJsonObject o = v.toObject();
        QPolygonF poly;
        const QJsonArray pts = o["points"].toArray();
        for (const QJsonValue &pv : pts) {
            const QJsonArray xy = pv.toArray();
            poly << QPointF(xy.at(0).toDouble(), xy.at(1).toDouble());
        }
        if (poly.size() < 2)
            continue;

        SymbolItem *sa, *sb;
        int pa, pb;
        endOf(o["a"], sa, pa);
        endOf(o["b"], sb, pb);

        auto *w = new WireItem;
        w->setPoints(poly);
        scene()->addItem(w);
        w->setEnds(sa, pa, sb, pb);
    }

    if (recenter && !scene()->items().isEmpty())
        centerOn(scene()->itemsBoundingRect().center());
    return true;
}

void SchemaView::commit()
{
    const QByteArray now = serialize();
    if (now == m_currentState)
        return;
    m_undo.append(m_currentState);
    if (m_undo.size() > 200)
        m_undo.removeFirst();
    m_redo.clear();
    m_currentState = now;
    emit stateChanged();
}

void SchemaView::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo.append(m_currentState);
    m_currentState = m_undo.takeLast();
    deserialize(m_currentState, false);
    emit stateChanged();
}

void SchemaView::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo.append(m_currentState);
    m_currentState = m_redo.takeLast();
    deserialize(m_currentState, false);
    emit stateChanged();
}

void SchemaView::clearAll()
{
    resetScene();
    m_undo.clear();
    m_redo.clear();
    m_currentState = serialize();
    m_saved = m_currentState;
    emit stateChanged();
}

bool SchemaView::saveToFile(const QString &path)
{
    commit();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(m_currentState);
    f.close();
    m_saved = m_currentState;
    emit stateChanged();
    return true;
}

bool SchemaView::loadFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    if (!deserialize(f.readAll(), true))
        return false;
    m_undo.clear();
    m_redo.clear();
    m_currentState = serialize();
    m_saved = m_currentState;
    emit stateChanged();
    return true;
}

// ------------------------------------------------------------------
//  Outil fil
// ------------------------------------------------------------------
bool SchemaView::pinAt(const QPointF &p, SymbolItem *&sym, int &pin) const
{
    const auto items = scene()->items();
    for (QGraphicsItem *it : items) {
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

bool SchemaView::onWire(const QPointF &p) const
{
    const auto items = scene()->items();
    for (QGraphicsItem *it : items) {
        auto *w = dynamic_cast<WireItem *>(it);
        if (w && w != m_current && w->touches(p))
            return true;
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
    if (pinAt(p, s, pin) || onWire(p)) {    // clic sur une broche ou sur un fil : on termine
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

void SchemaView::cancelWire()
{
    if (!m_current)
        return;
    delete m_current;
    m_current = nullptr;
    m_pts.clear();
}

void SchemaView::finishWire(bool keepLast)
{
    if (!m_current)
        return;
    if (!keepLast)
        m_pts.removeLast();                 // retire le segment d'aperçu

    if (m_pts.size() < 2) {
        cancelWire();
        return;
    }

    SymbolItem *sa = nullptr, *sb = nullptr;
    int pa = -1, pb = -1;
    pinAt(m_pts.first(), sa, pa);
    pinAt(m_pts.last(), sb, pb);
    m_current->setPoints(m_pts);
    m_current->setEnds(sa, pa, sb, pb);
    m_current = nullptr;
    m_pts.clear();
    commit();
}

// ------------------------------------------------------------------
//  Souris et clavier
// ------------------------------------------------------------------
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
    commit();                               // déplacement d'un composant, etc.
}

void SchemaView::mouseDoubleClickEvent(QMouseEvent *e)
{
    const QPoint pos = e->position().toPoint();
    if (m_wireMode) {
        if (e->button() == Qt::LeftButton)
            wireClick(mapToScene(pos));
        return;
    }
    if (e->button() == Qt::LeftButton) {
        if (auto *s = dynamic_cast<SymbolItem *>(itemAt(pos))) {
            PropertiesDialog dlg(s, this);
            dlg.exec();
            commit();
            return;
        }
    }
    if (e->button() == Qt::LeftButton && !itemAt(pos)) {
        SymbolItem *s = makeSymbol(m_kind);
        if (s) {
            s->index = ++m_total;
            s->ref = s->prefix() + QString::number(++m_counts[s->prefix()]);
            s->setPos(snap(mapToScene(pos)));
            scene()->addItem(s);
            commit();
        }
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(e);
}

void SchemaView::keyPressEvent(QKeyEvent *e)
{
    if (e->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) {
        QGraphicsView::keyPressEvent(e);      // laisse passer les raccourcis du menu
        return;
    }
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
        commit();
    } else if (e->key() == Qt::Key_Delete) {
        const auto items = scene()->selectedItems();
        for (QGraphicsItem *it : items)
            delete it;
        commit();
    } else {
        QGraphicsView::keyPressEvent(e);
    }
}

// ------------------------------------------------------------------
//  Netlist (touche N)
// ------------------------------------------------------------------
void SchemaView::dumpNets()
{
    QList<SymbolItem *> syms;
    QList<WireItem *> wires;
    collect(syms, wires);

    Schema *sc = buildSchema(syms, wires);

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

    QMessageBox::information(this, "Netlist", text);
    schema_free(sc);
}

// ------------------------------------------------------------------
//  Simulation : utilitaires
// ------------------------------------------------------------------
static QString ngspiceRoot()
{
    const QString base = QCoreApplication::applicationDirPath();
    QStringList roots = {base + "/ngspice"};
#ifdef Q_OS_MACOS
    roots.prepend(base + "/../Resources/ngspice");
#endif
    for (const QString &r : std::as_const(roots)) {
#ifdef Q_OS_WIN
        const QString exe = r + "/bin/ngspice_con.exe";
#else
        const QString exe = r + "/bin/ngspice";
#endif
        if (QFile::exists(exe))
            return QDir::cleanPath(r);
    }
    return QString();
}

static QString findNgspice()
{
    const QString root = ngspiceRoot();
    if (!root.isEmpty()) {
#ifdef Q_OS_WIN
        return root + "/bin/ngspice_con.exe";
#else
        return root + "/bin/ngspice";
#endif
    }
    return QStandardPaths::findExecutable("ngspice");   // secours (développement)
}

struct SpiceRun {
    QString out;     // contenu de out.txt (wrdata)
    QString log;     // contenu de out.log
    QString err;     // message d'erreur si échec
};

static SpiceRun runNgspice(const QString &net, int timeoutMs)
{
    SpiceRun r;
    const QString exe = findNgspice();
    if (exe.isEmpty()) {
        r.err = "ngspice est introuvable.";
        return r;
    }
    QTemporaryDir dir;
    if (!dir.isValid()) {
        r.err = "Impossible de créer un dossier temporaire.";
        return r;
    }
    QFile cir(dir.filePath("circuit.cir"));
    if (!cir.open(QIODevice::WriteOnly | QIODevice::Text)) {
        r.err = "Impossible d'écrire la netlist.";
        return r;
    }
    cir.write(net.toUtf8());
    cir.close();

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString root = ngspiceRoot();
    if (!root.isEmpty()) {
        env.insert("SPICE_SCRIPTS", root + "/share/ngspice/scripts");
#ifndef Q_OS_WIN
        env.insert("SPICE_LIB_DIR", root + "/share/ngspice");
#endif
    }
    proc.setProcessEnvironment(env);
    proc.setWorkingDirectory(dir.path());
    proc.start(exe, {"-b", "-o", "out.log", "circuit.cir"});
    if (!proc.waitForFinished(timeoutMs)) {
        if (proc.error() == QProcess::FailedToStart) {
            r.err = "ngspice n'a pas pu démarrer (bloqué par Windows ou un antivirus ?).";
        } else {
            proc.kill();
            proc.waitForFinished(1000);
            r.err = "ngspice ne répond pas (délai dépassé).";
        }
        return r;
    }

    QFile lg(dir.filePath("out.log"));
    if (lg.open(QIODevice::ReadOnly | QIODevice::Text))
        r.log = QString::fromUtf8(lg.readAll());
    else
        r.log = QString::fromUtf8(proc.readAllStandardOutput());
    QFile out(dir.filePath("out.txt"));
    if (out.open(QIODevice::ReadOnly | QIODevice::Text))
        r.out = QString::fromUtf8(out.readAll());
    return r;
}

// Lit un fichier wrdata : chaque vecteur occupe un groupe de colonnes dont la première
// est l'abscisse et la dernière la valeur (2 ou 3 colonnes selon le type d'analyse).
static bool parseColumns(const QString &text, int vectors, QVector<double> &x,
                         QList<QVector<double>> &ys)
{
    static const QRegularExpression ws("\\s+");
    x.clear();
    ys.clear();
    for (int k = 0; k < vectors; ++k)
        ys << QVector<double>();

    const QStringList lines = text.split('\n');
    for (const QString &line : lines) {
        const QStringList c = line.trimmed().split(ws, Qt::SkipEmptyParts);
        if (vectors <= 0 || c.size() < 2 * vectors || c.size() % vectors != 0)
            continue;
        const int stride = int(c.size()) / vectors;
        bool ok = true;
        const double xv = c[0].toDouble(&ok);
        if (!ok) continue;
        QVector<double> vals;
        for (int k = 0; k < vectors && ok; ++k)
            vals << c[k * stride + stride - 1].toDouble(&ok);
        if (!ok) continue;
        x << xv;
        for (int k = 0; k < vectors; ++k)
            ys[k] << vals[k];
    }
    return x.size() >= 2;
}

static const QRegularExpression &posNumber()
{
    static const QRegularExpression re(
        "^[0-9]+(\\.[0-9]+)?(e[-+]?[0-9]+)?(f|p|n|u|m|k|meg|g|t)?$",
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

static const QRegularExpression &signedNumber()
{
    static const QRegularExpression re(
        "^-?[0-9]+(\\.[0-9]+)?(e[-+]?[0-9]+)?(f|p|n|u|m|k|meg|g|t)?$",
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

static void showPlots(QWidget *parent, const QString &title, const QList<PlotWidget *> &plots)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    auto *lay = new QVBoxLayout(&dlg);
    for (PlotWidget *pl : plots)
        lay->addWidget(pl);
    dlg.resize(820, plots.size() > 1 ? 720 : 520);
    dlg.exec();
}

static QListWidget *addNodeList(QFormLayout *form, const SpiceResult &r)
{
    auto *lw = new QListWidget;
    lw->setMaximumHeight(140);
    for (const QString &n : r.nodes) {
        const QString d = r.nodeDesc.value(n);
        auto *it = new QListWidgetItem(d.isEmpty() ? n : QString("%1   (%2)").arg(n, d), lw);
        it->setData(Qt::UserRole, n);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
    }
    form->addRow("Courbes à tracer :", lw);
    return lw;
}

static QStringList checkedNodes(const QListWidget *lw)
{
    QStringList out;
    for (int i = 0; i < lw->count(); ++i)
        if (lw->item(i)->checkState() == Qt::Checked)
            out << lw->item(i)->data(Qt::UserRole).toString();
    return out;
}
// ------------------------------------------------------------------
//  Simulations
// ------------------------------------------------------------------
void SchemaView::simulateDC()
{
    QList<SymbolItem *> syms;
    QList<WireItem *> wires;
    collect(syms, wires);

    const SpiceResult r = buildNetlist(syms, wires);
    if (!r.error.isEmpty()) {
        QMessageBox::warning(this, "Simulation", r.error);
        return;
    }

    const SpiceRun run = runNgspice(r.netlist, 15000);
    if (!run.err.isEmpty()) {
        QMessageBox::warning(this, "Simulation", run.err);
        return;
    }

    const QString text = "=== Nœuds ===\n" + r.legend + "\n=== Netlist ===\n" + r.netlist
                         + "\n=== Résultat ngspice ===\n" + run.log;

    QDialog dlg(this);
    dlg.setWindowTitle("Simulation : point de fonctionnement DC");
    dlg.resize(700, 600);
    auto *lay = new QVBoxLayout(&dlg);
    auto *edit = new QPlainTextEdit(text);
    edit->setReadOnly(true);
    edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    lay->addWidget(edit);
    dlg.exec();
}

void SchemaView::simulateTran()
{
    QList<SymbolItem *> syms;
    QList<WireItem *> wires;
    collect(syms, wires);

    const SpiceResult r = buildNetlist(syms, wires);
    if (!r.error.isEmpty()) {
        QMessageBox::warning(this, "Simulation", r.error);
        return;
    }
    if (r.nodes.isEmpty()) {
        QMessageBox::warning(this, "Simulation", "Aucun nœud à tracer.");
        return;
    }

    QDialog pd(this);
    pd.setWindowTitle("Simulation transitoire");
    auto *form = new QFormLayout(&pd);
    auto *eStop = new QLineEdit("500u");
    auto *eStep = new QLineEdit("1u");
    form->addRow("Durée (ex. 5m, 500u) :", eStop);
    form->addRow("Pas (ex. 1u) :", eStep);
    QListWidget *list = addNodeList(form, r);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &pd, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &pd, &QDialog::reject);
    if (pd.exec() != QDialog::Accepted)
        return;

    const QStringList sel = checkedNodes(list);
    if (sel.isEmpty()) {
        QMessageBox::warning(this, "Simulation", "Cochez au moins une courbe.");
        return;
    }
    const QString stop = eStop->text().trimmed();
    const QString step = eStep->text().trimmed();
    if (!posNumber().match(stop).hasMatch() || !posNumber().match(step).hasMatch()) {
        QMessageBox::warning(this, "Simulation", "Valeur invalide. Exemples : 5m, 500u, 1u, 0.001");
        return;
    }

    const QString vecs = "v(" + sel.join(") v(") + ")";
    const QString net = r.body
                        + QString(".control\ntran %1 %2 uic\nwrdata out.txt %3\n.endc\n.end\n")
                              .arg(step, stop, vecs);

    const SpiceRun run = runNgspice(net, 30000);
    if (!run.err.isEmpty()) {
        QMessageBox::warning(this, "Simulation", run.err);
        return;
    }

    QVector<double> t;
    QList<QVector<double>> series;
    if (!parseColumns(run.out, int(sel.size()), t, series)) {
        QMessageBox::warning(this, "Simulation", "Aucun résultat.\n\n" + run.log.right(800));
        return;
    }

    QStringList names;
    for (const QString &n : sel)
        names << "V(" + n + ")";
    auto *plot = new PlotWidget;
    plot->setAxes("s", "V");
    plot->setData(t, series, names);
    showPlots(this, "Simulation transitoire", {plot});
}

void SchemaView::simulateSweep()
{
    QList<SymbolItem *> syms;
    QList<WireItem *> wires;
    collect(syms, wires);

    const SpiceResult r = buildNetlist(syms, wires);
    if (!r.error.isEmpty()) {
        QMessageBox::warning(this, "Simulation", r.error);
        return;
    }
    if (r.dcNames.isEmpty()) {
        QMessageBox::warning(this, "Simulation",
                             "Le balayage DC a besoin d'une source de tension DC dans le schéma.");
        return;
    }
    if (r.nodes.isEmpty()) {
        QMessageBox::warning(this, "Simulation", "Aucun nœud à tracer.");
        return;
    }

    QDialog pd(this);
    pd.setWindowTitle("Balayage DC");
    auto *form = new QFormLayout(&pd);
    auto *cb = new QComboBox;
    cb->addItems(r.dcRefs);
    auto *eStart = new QLineEdit("0");
    auto *eStop = new QLineEdit("5");
    auto *eStep = new QLineEdit("0.1");
    form->addRow("Source à faire varier :", cb);
    form->addRow("Début (V) :", eStart);
    form->addRow("Fin (V) :", eStop);
    form->addRow("Pas (V) :", eStep);
    QListWidget *list = addNodeList(form, r);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &pd, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &pd, &QDialog::reject);
    if (pd.exec() != QDialog::Accepted)
        return;

    const QStringList sel = checkedNodes(list);
    if (sel.isEmpty()) {
        QMessageBox::warning(this, "Simulation", "Cochez au moins une courbe.");
        return;
    }
    const QString a = eStart->text().trimmed(), b = eStop->text().trimmed(),
        c = eStep->text().trimmed();
    if (!signedNumber().match(a).hasMatch() || !signedNumber().match(b).hasMatch()
        || !posNumber().match(c).hasMatch()) {
        QMessageBox::warning(this, "Simulation",
                             "Valeur invalide. Exemples : 0, 5, -2, 0.1 (le pas doit être positif).");
        return;
    }

    const QString vecs = "v(" + sel.join(") v(") + ")";
    const QString net = r.body
                        + QString(".control\ndc %1 %2 %3 %4\nwrdata out.txt %5\n.endc\n.end\n")
                              .arg(r.dcNames[cb->currentIndex()], a, b, c, vecs);

    const SpiceRun run = runNgspice(net, 30000);
    if (!run.err.isEmpty()) {
        QMessageBox::warning(this, "Simulation", run.err);
        return;
    }

    QVector<double> x;
    QList<QVector<double>> series;
    if (!parseColumns(run.out, int(sel.size()), x, series)) {
        QMessageBox::warning(this, "Simulation", "Aucun résultat.\n\n" + run.log.right(800));
        return;
    }

    QStringList names;
    for (const QString &n : sel)
        names << "V(" + n + ")";
    auto *plot = new PlotWidget;
    plot->setAxes("V", "V");
    plot->setData(x, series, names);
    showPlots(this, "Balayage DC : " + cb->currentText(), {plot});
}

void SchemaView::simulateAC()
{
    QList<SymbolItem *> syms;
    QList<WireItem *> wires;
    collect(syms, wires);

    const SpiceResult r = buildNetlist(syms, wires, true);
    if (!r.error.isEmpty()) {
        QMessageBox::warning(this, "Simulation", r.error);
        return;
    }
    if (!r.hasAc) {
        QMessageBox::warning(this, "Simulation",
                             "L'analyse AC a besoin d'une source sinusoïdale : c'est l'entrée du "
                             "circuit (1 V en AC).");
        return;
    }
    if (r.nodes.isEmpty()) {
        QMessageBox::warning(this, "Simulation", "Aucun nœud à tracer.");
        return;
    }

    QDialog pd(this);
    pd.setWindowTitle("Analyse AC (Bode)");
    auto *form = new QFormLayout(&pd);
    auto *eStart = new QLineEdit("10");
    auto *eStop = new QLineEdit("100k");
    auto *ePts = new QLineEdit("20");
    form->addRow("Fréquence de début (Hz) :", eStart);
    form->addRow("Fréquence de fin (Hz) :", eStop);
    form->addRow("Points par décade :", ePts);
    QListWidget *list = addNodeList(form, r);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &pd, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &pd, &QDialog::reject);
    if (pd.exec() != QDialog::Accepted)
        return;

    const QStringList sel = checkedNodes(list);
    if (sel.isEmpty()) {
        QMessageBox::warning(this, "Simulation", "Cochez au moins une courbe.");
        return;
    }
    const QString f1 = eStart->text().trimmed(), f2 = eStop->text().trimmed(),
        np = ePts->text().trimmed();
    static const QRegularExpression integer("^[0-9]{1,4}$");
    if (!posNumber().match(f1).hasMatch() || !posNumber().match(f2).hasMatch()
        || !integer.match(np).hasMatch() || f1 == "0") {
        QMessageBox::warning(this, "Simulation",
                             "Valeur invalide. Exemples : 10, 100k, 1meg ; points par décade : 20.");
        return;
    }

    QStringList vecs;
    for (const QString &n : sel)
        vecs << QString("db(v(%1)) phase(v(%1))").arg(n);
    const QString net = r.body
                        + QString(".control\nac dec %1 %2 %3\nwrdata out.txt %4\n.endc\n.end\n")
                              .arg(np, f1, f2, vecs.join(' '));

    const SpiceRun run = runNgspice(net, 30000);
    if (!run.err.isEmpty()) {
        QMessageBox::warning(this, "Simulation", run.err);
        return;
    }

    QVector<double> f;
    QList<QVector<double>> raw;
    const int nodes = int(sel.size());
    if (!parseColumns(run.out, 2 * nodes, f, raw)) {
        QMessageBox::warning(this, "Simulation", "Aucun résultat.\n\n" + run.log.right(800));
        return;
    }

    QList<QVector<double>> gain, phase;
    QStringList names;
    for (int k = 0; k < nodes; ++k) {
        gain << raw[2 * k];
        QVector<double> ph = raw[2 * k + 1];
        for (double &v : ph)
            v = v * 180.0 / M_PI;                 // radians -> degrés
        for (int i = 1; i < ph.size(); ++i) {     // déroulement de la phase
            while (ph[i] - ph[i - 1] > 180.0) ph[i] -= 360.0;
            while (ph[i] - ph[i - 1] < -180.0) ph[i] += 360.0;
        }
        phase << ph;
        names << "V(" + sel[k] + ")";
    }

    auto *pg = new PlotWidget;
    pg->setAxes("Hz", "dB", true, false);
    pg->setData(f, gain, names);
    auto *pp = new PlotWidget;
    pp->setAxes("Hz", "°", true, false);
    pp->setData(f, phase, names);
    showPlots(this, "Analyse AC : gain (dB) et phase (°)", {pg, pp});
}