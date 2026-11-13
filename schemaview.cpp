#include "schemaview.h"
#include "symbolitem.h"
#include "wireitem.h"
#include "wiredialog.h"
#include "propertiesdialog.h"
#include "sheet.h"
#include "spice.h"
#include "core.h"

#include <QTimer>
#include <QDate>
#include <QFile>
#include <QGraphicsScene>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMap>
#include <QMarginsF>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QScrollBar>
#include <QSet>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <functional>

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

static QString propOf(const SymbolItem *s, const QString &name)
{
    for (const Prop &p : s->props)
        if (p.name == name)
            return p.value.trimmed();
    return QString();
}

// ------------------------------------------------------------------
//  Construction, fond, zoom
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

    Folio f;
    f.title = "Folio 1";
    m_folios.append(f);
    ensureSheet();
    m_currentState = serialize();
    m_folios[0].state = m_currentState;
    m_dirty = false;
}

void SchemaView::drawBackground(QPainter *p, const QRectF &rect)
{
    p->fillRect(rect, QColor(226, 229, 234));
    if (m_sheetOn || transform().m11() < 0.3)     // avec la feuille, la grille est dessinée par elle
        return;
    p->setPen(QPen(QColor(150, 155, 165), 0));
    const int x0 = qFloor(rect.left() / GRID), x1 = qCeil(rect.right() / GRID);
    const int y0 = qFloor(rect.top() / GRID), y1 = qCeil(rect.bottom() / GRID);
    for (int ix = x0; ix <= x1; ++ix)
        for (int iy = y0; iy <= y1; ++iy)
            p->drawPoint(QPointF(ix * GRID, iy * GRID));
}

void SchemaView::drawJunctions(QPainter *p) const
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

void SchemaView::drawForeground(QPainter *p, const QRectF &)
{
    drawJunctions(p);
}

void SchemaView::wheelEvent(QWheelEvent *e)
{
    const qreal factor = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const qreal next = transform().m11() * factor;
    if (next < 0.1 || next > 10.0)
        return;
    scale(factor, factor);
}

void SchemaView::zoomFit()
{
    const QSizeF sz = SheetItem::paperSize(m_pi.paper);
    fitInView(QRectF(QPointF(0, 0), sz).adjusted(-30, -30, 30, 30), Qt::KeepAspectRatio);
}

void SchemaView::setSheetEnabled(bool on)
{
    if (on == m_sheetOn)
        return;
    m_sheetOn = on;
    applySheet(m_folio);
    if (on)
        QTimer::singleShot(0, this, [this] { zoomFit(); });
    else {
        resetTransform();
        centerOn(600, 420);
    }
    scene()->update();
    viewport()->update();
}
// ------------------------------------------------------------------
//  Feuille et folios
// ------------------------------------------------------------------
void SchemaView::ensureSheet()
{
    m_sheet = new SheetItem;
    scene()->addItem(m_sheet);
    applySheet(m_folio);
}

void SchemaView::applySheet(int index)
{
    if (!m_sheet || m_folios.isEmpty())
        return;
    const QString t = (index >= 0 && index < m_folios.size()) ? m_folios[index].title : QString();
    m_sheet->setInfo(m_pi, index, int(m_folios.size()), t);
    m_sheet->setVisible(m_sheetOn || m_exporting);   // l'export PDF utilise toujours la feuille
    m_sheet->setShowGrid(!m_exporting);
}

QString SchemaView::folioTitle(int i) const
{
    return (i >= 0 && i < m_folios.size()) ? m_folios[i].title : QString();
}

void SchemaView::storeCurrent()
{
    if (m_folio < 0 || m_folio >= m_folios.size())
        return;
    Folio &f = m_folios[m_folio];
    f.state = m_currentState;
    f.undo = m_undo;
    f.redo = m_redo;
}

void SchemaView::loadFolio(int i)
{
    m_folio = i;
    const Folio &f = m_folios[i];
    m_currentState = f.state;
    m_undo = f.undo;
    m_redo = f.redo;
    deserialize(m_currentState, false);
    applySheet(i);
}

void SchemaView::setFolio(int i)
{
    if (i < 0 || i >= m_folios.size() || i == m_folio)
        return;
    commit();
    storeCurrent();
    loadFolio(i);
    emit foliosChanged();
    emit stateChanged();
}

void SchemaView::addFolio()
{
    commit();
    storeCurrent();
    Folio f;
    f.title = QString("Folio %1").arg(m_folios.size() + 1);
    f.state = emptyState();
    m_folios.append(f);
    m_dirty = true;
    loadFolio(int(m_folios.size()) - 1);
    emit foliosChanged();
    emit stateChanged();
}

bool SchemaView::removeFolio()
{
    if (m_folios.size() <= 1)
        return false;
    m_folios.removeAt(m_folio);
    m_dirty = true;
    loadFolio(qMin(m_folio, int(m_folios.size()) - 1));
    emit foliosChanged();
    emit stateChanged();
    return true;
}

void SchemaView::renameFolio(int i, const QString &title)
{
    if (i < 0 || i >= m_folios.size())
        return;
    m_folios[i].title = title.trimmed();
    m_dirty = true;
    if (i == m_folio)
        applySheet(i);
    emit foliosChanged();
    emit stateChanged();
}

void SchemaView::setProject(const ProjectInfo &pi)
{
    const bool paperChanged = pi.paper != m_pi.paper;
    m_pi = pi;
    m_dirty = true;
    applySheet(m_folio);
    if (paperChanged)
        zoomFit();
    emit stateChanged();
}

// exécute fn pour chaque folio (la scène affiche le folio i pendant l'appel)
void SchemaView::forEachFolio(const std::function<void(int)> &fn)
{
    commit();
    storeCurrent();
    for (int i = 0; i < m_folios.size(); ++i) {
        deserialize(m_folios[i].state, false);
        applySheet(i);
        fn(i);
    }
    deserialize(m_folios[m_folio].state, false);
    applySheet(m_folio);
}

// ------------------------------------------------------------------
//  Historique et sérialisation d'un folio
// ------------------------------------------------------------------
void SchemaView::collect(QList<SymbolItem *> &syms, QList<WireItem *> &wires) const
{
    const auto items = scene()->items();
    for (QGraphicsItem *it : items) {
        if (auto *s = dynamic_cast<SymbolItem *>(it))
            syms << s;
        else if (auto *w = dynamic_cast<WireItem *>(it)) {
            if (w != m_current)
                wires << w;
        }
    }
    std::sort(syms.begin(), syms.end(),
              [](SymbolItem *a, SymbolItem *b) { return a->index < b->index; });
}

QByteArray SchemaView::emptyState()
{
    QJsonObject o;
    o["format"] = "voltix";
    o["version"] = 1;
    o["components"] = QJsonArray();
    o["wires"] = QJsonArray();
    return QJsonDocument(o).toJson();
}

bool SchemaView::validState(const QByteArray &data)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    return err.error == QJsonParseError::NoError && doc.isObject()
           && doc.object()["format"].toString() == "voltix";
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
        o["nature"] = w->nature();
        o["color"] = w->color().name();
        o["section"] = w->section();
        o["mark"] = w->mark();
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

void SchemaView::resetCounters()
{
    m_total = 0;
    m_counts.clear();
}

// met à jour les compteurs de repères avec un folio (les repères sont uniques dans tout le projet)
void SchemaView::scanCounters(const QByteArray &data)
{
    const QJsonArray comps = QJsonDocument::fromJson(data).object()["components"].toArray();
    for (const QJsonValue &v : comps) {
        const QJsonObject o = v.toObject();
        m_total = qMax(m_total, o["index"].toInt());
        const QString ref = o["ref"].toString();
        int d = int(ref.size());
        while (d > 0 && ref[d - 1].isDigit())
            --d;
        if (d < ref.size())
            m_counts[ref.left(d)] = qMax(m_counts.value(ref.left(d)), ref.mid(d).toInt());
    }
}

void SchemaView::resetScene()
{
    cancelWire();
    m_sheet = nullptr;
    scene()->clear();
    ensureSheet();
}

bool SchemaView::deserialize(const QByteArray &data, bool recenter)
{
    if (!validState(data))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(data).object();

    resetScene();

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
        w->setNature(o["nature"].toString("OTHER"));
        const QColor c(o["color"].toString());
        if (c.isValid())
            w->setColor(c);
        w->setSection(o["section"].toString());
        w->setMark(o["mark"].toString());
        scene()->addItem(w);
        w->setEnds(sa, pa, sb, pb);
    }

    if (recenter)
        zoomFit();
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
    m_dirty = true;
    emit stateChanged();
}

void SchemaView::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo.append(m_currentState);
    m_currentState = m_undo.takeLast();
    deserialize(m_currentState, false);
    m_dirty = true;
    emit stateChanged();
}

void SchemaView::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo.append(m_currentState);
    m_currentState = m_redo.takeLast();
    deserialize(m_currentState, false);
    m_dirty = true;
    emit stateChanged();
}

// ------------------------------------------------------------------
//  Projet : nouveau, enregistrement, chargement, PDF
// ------------------------------------------------------------------
void SchemaView::clearAll()
{
    const ProjectInfo old = m_pi;
    m_pi = ProjectInfo();
    m_pi.company = old.company;
    m_pi.address = old.address;
    m_pi.author = old.author;
    m_pi.paper = old.paper;

    m_folios.clear();
    Folio f;
    f.title = "Folio 1";
    f.state = emptyState();
    m_folios.append(f);
    m_folio = 0;
    m_undo.clear();
    m_redo.clear();
    m_currentState = f.state;
    resetCounters();
    resetScene();
    applySheet(0);
    m_dirty = false;
    zoomFit();
    emit foliosChanged();
    emit stateChanged();
}

bool SchemaView::saveToFile(const QString &path)
{
    commit();
    storeCurrent();
    m_pi.modified = QDate::currentDate().toString(Qt::ISODate);
    applySheet(m_folio);

    QJsonObject pj;
    pj["paper"] = m_pi.paper;
    pj["company"] = m_pi.company;
    pj["address"] = m_pi.address;
    pj["title"] = m_pi.title;
    pj["author"] = m_pi.author;
    pj["revision"] = m_pi.revision;
    pj["created"] = m_pi.created;
    pj["modified"] = m_pi.modified;

    QJsonArray fl;
    for (const Folio &fo : std::as_const(m_folios)) {
        QJsonObject o;
        o["title"] = fo.title;
        o["data"] = QJsonDocument::fromJson(fo.state).object();
        fl.append(o);
    }

    QJsonObject root;
    root["format"] = "voltix";
    root["version"] = 2;
    root["project"] = pj;
    root["folios"] = fl;
    root["current"] = m_folio;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson());
    file.close();
    m_dirty = false;
    emit stateChanged();
    return true;
}

bool SchemaView::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray raw = file.readAll();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    if (root["format"].toString() != "voltix")
        return false;

    QList<Folio> folios;
    ProjectInfo pi;
    int cur = 0;
    if (root.contains("folios")) {                          // format 2
        const QJsonObject pj = root["project"].toObject();
        pi.paper = pj["paper"].toString("A4");
        pi.company = pj["company"].toString();
        pi.address = pj["address"].toString();
        pi.title = pj["title"].toString();
        pi.author = pj["author"].toString();
        pi.revision = pj["revision"].toString("A");
        pi.created = pj["created"].toString(pi.created);
        pi.modified = pj["modified"].toString(pi.modified);
        const QJsonArray fl = root["folios"].toArray();
        for (const QJsonValue &v : fl) {
            const QJsonObject fo = v.toObject();
            QJsonObject data = fo["data"].toObject();
            data["format"] = "voltix";
            Folio f;
            f.title = fo["title"].toString(QString("Folio %1").arg(folios.size() + 1));
            f.state = QJsonDocument(data).toJson();
            folios.append(f);
        }
        cur = root["current"].toInt(0);
    } else {                                                // ancien format : un seul folio
        Folio f;
        f.title = "Folio 1";
        f.state = raw;
        folios.append(f);
    }
    if (folios.isEmpty())
        return false;
    for (const Folio &f : std::as_const(folios))
        if (!validState(f.state))
            return false;
    if (pi.paper != "A3")
        pi.paper = "A4";
    if (cur < 0 || cur >= folios.size())
        cur = 0;

    m_pi = pi;
    m_folios = folios;
    resetCounters();
    for (const Folio &f : std::as_const(m_folios))
        scanCounters(f.state);
    loadFolio(cur);
    m_dirty = false;
    zoomFit();
    emit foliosChanged();
    emit stateChanged();
    return true;
}

bool SchemaView::exportPdf(const QString &path)
{
    commit();
    storeCurrent();
    m_pi.modified = QDate::currentDate().toString(Qt::ISODate);

    QPdfWriter w(path);
    w.setPageSize(QPageSize(m_pi.paper == "A3" ? QPageSize::A3 : QPageSize::A4));
    w.setPageOrientation(QPageLayout::Landscape);
    w.setPageMargins(QMarginsF(0, 0, 0, 0));
    w.setResolution(300);
    w.setTitle(m_pi.title.isEmpty() ? QString("Schéma Voltix") : m_pi.title);
    w.setCreator("Voltix");

    QPainter painter(&w);
    if (!painter.isActive())
        return false;
    painter.setRenderHint(QPainter::Antialiasing);

    m_exporting = true;
    SymbolItem::hidePins = true;
    const QRectF source(QPointF(0, 0), SheetItem::paperSize(m_pi.paper));
    for (int i = 0; i < m_folios.size(); ++i) {
        if (i > 0)
            w.newPage();
        deserialize(m_folios[i].state, false);
        applySheet(i);
        scene()->clearSelection();
        const QRectF target(painter.viewport());
        scene()->render(&painter, target, source, Qt::KeepAspectRatio);

        const qreal sc = qMin(target.width() / source.width(), target.height() / source.height());
        painter.save();
        painter.translate(target.center() - QPointF(source.width() * sc / 2, source.height() * sc / 2));
        painter.scale(sc, sc);
        drawJunctions(&painter);                       // les points de jonction
        painter.restore();
    }
    painter.end();
    SymbolItem::hidePins = false;
    m_exporting = false;
    loadFolio(m_folio);
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
            if (!s->pinActive(i))
                continue;
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

    if (!m_current) {
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
    if (pinAt(p, s, pin) || onWire(p)) {
        if (p == last)
            return;
        if (!qFuzzyCompare(p.x() + 1, last.x() + 1) && !qFuzzyCompare(p.y() + 1, last.y() + 1)) {
            m_pts.last() = QPointF(p.x(), last.y());
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
    m_pts.last() = end;
    m_pts << end;
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
        m_pts.removeLast();

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
    m_current->setNature(m_wireNature);             // nature et section choisies dans la barre Fils
    m_current->setSection(m_wireSection);
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
    commit();
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
        QGraphicsItem *hit = itemAt(pos);
        if (auto *s = dynamic_cast<SymbolItem *>(hit)) {
            PropertiesDialog dlg(s, this);
            dlg.exec();
            commit();
            return;
        }
        if (auto *w = dynamic_cast<WireItem *>(hit)) {
            WireDialog dlg(w, this);
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
        QGraphicsView::keyPressEvent(e);
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
//  Netlist (touche N) et vérification (F9)
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
            if (!syms[c]->pinActive(i))
                continue;
            const QString name = QString("%1.%2").arg(syms[c]->ref).arg(i + 1);
            const int n = schema_pin_net(sc, c, i);
            if (n >= 0) nets[n] << name;
            else        floating << name;
        }
    }

    QString text = QString("%1 net(s)\n\n").arg(nets.size());
    for (auto it = nets.cbegin(); it != nets.cend(); ++it)
        text += QString("Net %1 : %2\n").arg(it.key()).arg(it.value().join(", "));
    if (!floating.isEmpty())
        text += QString("\nNon connectées : %1").arg(floating.join(", "));

    QMessageBox::information(this, "Netlist (folio courant)", text);
    schema_free(sc);
}

void SchemaView::verifyElectrical()
{
    struct Rec { QString kind, ref, bobine; };
    QList<Rec> recs;
    QStringList issues;

    forEachFolio([&](int f) {
        QList<SymbolItem *> syms;
        QList<WireItem *> all, wires;
        collect(syms, all);
        for (WireItem *w : std::as_const(all))
            if (w->points().size() >= 2)
                wires << w;
        for (const SymbolItem *s : std::as_const(syms))
            recs.append(Rec{s->kind(), s->ref.trimmed(), propOf(s, "Bobine")});

        Schema *sc = buildSchema(syms, wires);

        QStringList floating;
        for (int c = 0; c < syms.size(); ++c)
            for (int i = 0; i < syms[c]->pinCount(); ++i)
                if (syms[c]->pinActive(i) && schema_pin_net(sc, c, i) < 0)
                    floating << QString("%1.%2").arg(syms[c]->ref).arg(i + 1);
        if (!floating.isEmpty())
            issues << QString("Folio %1 : broches non connectées : %2").arg(f + 1).arg(floating.join(", "));

        // un même net ne doit pas mélanger des natures de fils différentes
        QMap<int, QSet<QString>> nat;
        for (int w = 0; w < wires.size(); ++w) {
            if (wires[w]->nature() == "OTHER")
                continue;
            const int net = schema_wire_net(sc, w);
            if (net >= 0)
                nat[net] << wires[w]->nature();
        }
        for (auto it = nat.cbegin(); it != nat.cend(); ++it)
            if (it.value().size() > 1) {
                QStringList lab;
                for (const QString &n : it.value())
                    lab << wireNatureLabel(n);
                lab.sort();
                issues << QString("Folio %1 : un même net mélange %2.").arg(f + 1).arg(lab.join(" et "));
            }
        schema_free(sc);
    });

    // cohérence des repères dans tout le projet
    static const QSet<QString> contactKinds = {"contact_no", "contact_nc", "contact_co",
                                               "contact_td_no", "contact_td_nc", "thermal_nc"};
    QSet<QString> coils, contactors, thermals, used;
    for (const Rec &r : std::as_const(recs)) {
        const QString key = r.ref.toLower();
        if (r.kind == "coil" || r.kind == "relay" || r.kind == "timer_coil") {
            if (coils.contains(key))
                issues << QString("Bobine « %1 » en double.").arg(r.ref);
            coils << key;
        } else if (r.kind.startsWith("contactor")) {
            contactors << key;
        } else if (r.kind == "thermal3") {
            thermals << key;
        }
    }
    for (const Rec &r : std::as_const(recs)) {
        if (!contactKinds.contains(r.kind))
            continue;
        const QString key = r.bobine.toLower();
        if (key.isEmpty()) {
            issues << QString("Contact sans bobine associée (%1).").arg(r.ref);
            continue;
        }
        const bool ok = (r.kind == "thermal_nc") ? thermals.contains(key) : coils.contains(key);
        if (!ok)
            issues << QString("Contact « %1 » : aucun repère « %2 » dans le projet.")
                          .arg(r.ref, r.bobine);
        used << key;
    }
    for (const Rec &r : std::as_const(recs)) {
        const QString key = r.ref.toLower();
        if ((r.kind == "coil" || r.kind == "relay" || r.kind == "timer_coil")
            && !used.contains(key) && !contactors.contains(key))
            issues << QString("Bobine « %1 » : aucun contact ni pôle de puissance associé.").arg(r.ref);
        if (r.kind == "thermal3" && !used.contains(key))
            issues << QString("Relais thermique « %1 » : aucun contact thermique associé.").arg(r.ref);
    }

    QMessageBox::information(this, "Vérification du projet",
                             issues.isEmpty() ? QString("Aucun problème détecté.")
                                              : issues.join("\n"));
}