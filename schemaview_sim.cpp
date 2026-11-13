#include "schemaview.h"
#include "symbolitem.h"
#include "wireitem.h"
#include "plotwidget.h"
#include "spice.h"
#include "core.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ------------------------------------------------------------------
//  ngspice : recherche et exécution
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
    lw->setMaximumHeight(170);
    for (int i = 0; i < r.meters.size(); ++i) {              // instruments posés sur le schéma
        const SpiceMeter &m = r.meters[i];
        auto *it = new QListWidgetItem(QString("%1   (%2 : %3)").arg(m.label, m.ref,
                                                                      m.unit == "A" ? "ampèremètre" : "voltmètre"), lw);
        it->setData(Qt::UserRole, QString("@%1").arg(i));
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
    }
    for (const QString &n : r.nodes) {                        // tensions de nœud (par rapport à la masse)
        const QString d = r.nodeDesc.value(n);
        auto *it = new QListWidgetItem(d.isEmpty() ? n : QString("%1   (%2)").arg(n, d), lw);
        it->setData(Qt::UserRole, n);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(r.meters.isEmpty() ? Qt::Checked : Qt::Unchecked);
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

// Courbes choisies : instruments ("@i") ou tensions de nœud
struct Picked {
    QStringList vec;       // vecteurs ngspice à écrire
    QStringList lets;      // définitions des instruments
    QStringList names;     // légende
    QStringList units;     // "V" ou "A"
};

static Picked pickCurves(const SpiceResult &r, const QStringList &sel)
{
    Picked p;
    for (const QString &s : sel) {
        if (s.startsWith('@')) {
            const int i = s.mid(1).toInt();
            if (i < 0 || i >= r.meters.size()) continue;
            p.lets << QString("let m%1 = %2").arg(i).arg(r.meters[i].expr);
            p.vec << QString("m%1").arg(i);
            p.names << r.meters[i].label;
            p.units << r.meters[i].unit;
        } else {
            p.vec << "v(" + s + ")";
            p.names << "V(" + s + ")";
            p.units << "V";
        }
    }
    return p;
}

// une courbe par unité : tensions en haut, courants en dessous
static void plotByUnit(QWidget *parent, const QString &title, const QString &xlabel,
                       const QVector<double> &x, const QList<QVector<double>> &ys,
                       const Picked &pk)
{
    QList<PlotWidget *> plots;
    for (const QString &u : QStringList{"V", "A"}) {
        QList<QVector<double>> sub;
        QStringList nm;
        for (int k = 0; k < ys.size(); ++k)
            if (pk.units.value(k) == u) { sub << ys[k]; nm << pk.names[k]; }
        if (sub.isEmpty()) continue;
        auto *pl = new PlotWidget;
        pl->setAxes(xlabel, u);
        pl->setData(x, sub, nm);
        plots << pl;
    }
    showPlots(parent, title, plots);
}

// ------------------------------------------------------------------
//  Simulations (folio courant)
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

    // valeurs des instruments : lignes « m0 = 1.2e+01 » du journal ngspice
    QString mesures;
    for (int i = 0; i < r.meters.size(); ++i) {
        const SpiceMeter &m = r.meters[i];
        const QRegularExpression re(QString("(?m)^\\s*m%1\\s*=\\s*(\\S+)").arg(i));
        const QRegularExpressionMatch mt = re.match(run.log);
        QString shown;
        if (mt.hasMatch()) {
            bool ok = false;
            const double v = mt.captured(1).toDouble(&ok);
            if (ok) shown = QString("%1 %2").arg(v, 0, 'g', 5).arg(m.unit);
        }
        if (auto *mi = dynamic_cast<MeterItem *>(m.sym)) { mi->reading = shown; mi->update(); }
        mesures += QString("%1 (%2) = %3\n").arg(m.label, m.ref, shown.isEmpty() ? "?" : shown);
    }
    const QString text = (mesures.isEmpty() ? QString() : "=== Mesures ===\n" + mesures + "\n")
                         + "=== Nœuds ===\n" + r.legend + "\n=== Netlist ===\n" + r.netlist
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

    const Picked pk = pickCurves(r, sel);
    const QString net = r.body
                        + QString(".control\ntran %1 %2 uic\n%3\nwrdata out.txt %4\n.endc\n.end\n")
                              .arg(step, stop, pk.lets.join('\n'), pk.vec.join(' '));

    const SpiceRun run = runNgspice(net, 30000);
    if (!run.err.isEmpty()) {
        QMessageBox::warning(this, "Simulation", run.err);
        return;
    }

    QVector<double> t;
    QList<QVector<double>> series;
    if (!parseColumns(run.out, int(pk.vec.size()), t, series)) {
        QMessageBox::warning(this, "Simulation", "Aucun résultat.\n\n" + run.log.right(800));
        return;
    }

    plotByUnit(this, "Simulation transitoire", "s", t, series, pk);
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

    const Picked pk = pickCurves(r, sel);
    const QString net = r.body
                        + QString(".control\ndc %1 %2 %3 %4\n%5\nwrdata out.txt %6\n.endc\n.end\n")
                              .arg(r.dcNames[cb->currentIndex()], a, b, c, pk.lets.join('\n'), pk.vec.join(' '));

    const SpiceRun run = runNgspice(net, 30000);
    if (!run.err.isEmpty()) {
        QMessageBox::warning(this, "Simulation", run.err);
        return;
    }

    QVector<double> x;
    QList<QVector<double>> series;
    if (!parseColumns(run.out, int(pk.vec.size()), x, series)) {
        QMessageBox::warning(this, "Simulation", "Aucun résultat.\n\n" + run.log.right(800));
        return;
    }

    plotByUnit(this, "Balayage DC : " + cb->currentText(), "V", x, series, pk);
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

    const Picked pk = pickCurves(r, sel);
    QStringList vecs;
    for (const QString &v : pk.vec)
        vecs << QString("db(%1) phase(%1)").arg(v);
    const QString net = r.body
                        + QString(".control\nac dec %1 %2 %3\n%4\nwrdata out.txt %5\n.endc\n.end\n")
                              .arg(np, f1, f2, pk.lets.join('\n'), vecs.join(' '));

    const SpiceRun run = runNgspice(net, 30000);
    if (!run.err.isEmpty()) {
        QMessageBox::warning(this, "Simulation", run.err);
        return;
    }

    QVector<double> f;
    QList<QVector<double>> raw;
    const int nodes = int(pk.vec.size());
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
        names << pk.names[k];
    }

    auto *pg = new PlotWidget;
    pg->setAxes("Hz", "dB", true, false);
    pg->setData(f, gain, names);
    auto *pp = new PlotWidget;
    pp->setAxes("Hz", "°", true, false);
    pp->setData(f, phase, names);
    showPlots(this, "Analyse AC : gain (dB) et phase (°)", {pg, pp});
}