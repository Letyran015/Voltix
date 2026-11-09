#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "schemaview.h"
#include "updatechecker.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto *view = new SchemaView(this);
    m_view = view;
    setCentralWidget(view);

    // Barre d'outils : choix du composant posé au double-clic
    auto *tb = addToolBar("Composants");
    auto *group = new QActionGroup(this);
    const QStringList names = {"Résistance", "Condensateur", "Inductance", "Diode",
                               "Source DC", "Source sinus", "Source impulsion",
                               "NPN", "PNP", "Ampli-op", "Masse", "Étiquette"};
    const QStringList ids   = {"resistor", "capacitor", "inductor", "diode",
                             "vsource", "vsin", "vpulse",
                             "npn", "pnp", "opamp", "ground", "label"};
    for (int i = 0; i < names.size(); ++i) {
        QAction *a = tb->addAction(names[i]);
        a->setCheckable(true);
        group->addAction(a);
        if (i == 0)
            a->setChecked(true);
        const QString id = ids[i];
        connect(a, &QAction::triggered, this, [view, id] { view->setKind(id); });
    }

    // Menu Fichier
    QMenu *fm = menuBar()->addMenu("&Fichier");
    fm->addAction("&Nouveau", QKeySequence::New, this, &MainWindow::newFile);
    fm->addAction("&Ouvrir…", QKeySequence::Open, this, &MainWindow::openFile);
    fm->addSeparator();
    fm->addAction("&Enregistrer", QKeySequence::Save, this, &MainWindow::saveFile);
    fm->addAction("Enregistrer &sous…", QKeySequence::SaveAs, this, &MainWindow::saveFileAs);

    // Menu Édition
    QMenu *em = menuBar()->addMenu("&Édition");
    m_undoAct = em->addAction("&Annuler", QKeySequence::Undo, this, [view] { view->undo(); });
    m_redoAct = em->addAction("&Rétablir", QKeySequence::Redo, this, [view] { view->redo(); });

    // Menu Simulation
    QMenu *sm = menuBar()->addMenu("&Simulation");
    sm->addAction("Point de fonctionnement &DC", QKeySequence(Qt::Key_F5), this,
                  [view] { view->simulateDC(); });
    sm->addAction("&Transitoire…", QKeySequence(Qt::Key_F6), this,
                  [view] { view->simulateTran(); });
    sm->addAction("&Balayage DC…", QKeySequence(Qt::Key_F7), this,
                  [view] { view->simulateSweep(); });
    sm->addAction("Analyse &AC (Bode)…", QKeySequence(Qt::Key_F8), this,
                  [view] { view->simulateAC(); });

    // Menu Aide + mises à jour
    auto *updater = new UpdateChecker(this);
    QMenu *hm = menuBar()->addMenu("&Aide");
    hm->addAction("Rechercher des mises à jour…", this, [updater] { updater->check(true); });

    QAction *autoAct = hm->addAction("Vérifier automatiquement les mises à jour");
    autoAct->setCheckable(true);
    autoAct->setChecked(QSettings().value("update/auto", true).toBool());
    connect(autoAct, &QAction::toggled, this,
            [](bool on) { QSettings().setValue("update/auto", on); });

    hm->addSeparator();
    hm->addAction("À &propos de Voltix", this, [this] {
        QMessageBox::about(this, "À propos de Voltix",
                           "<b>Voltix</b> " + UpdateChecker::currentVersion() + "<br>"
                                                                                "Éditeur de schémas avec simulation.<br><br>"
                                                                                "Simulation : <a href='https://ngspice.sourceforge.io'>ngspice</a> (licence BSD).<br>"
                                                                                "Interface : <a href='https://www.qt.io'>Qt</a> (LGPL).<br>"
                                                                                "Les textes de licence sont dans le dossier « licenses ».");
    });

    connect(view, &SchemaView::stateChanged, this, &MainWindow::updateState);
    updateState();

    // vérification silencieuse peu après le démarrage
    QTimer::singleShot(3000, this, [updater] { updater->check(false); });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateState()
{
    const QString name = m_path.isEmpty() ? QString("Sans titre") : QFileInfo(m_path).fileName();
    setWindowTitle(QString("Voltix - %1%2").arg(name, m_view->isModified() ? " *" : ""));
    m_undoAct->setEnabled(m_view->canUndo());
    m_redoAct->setEnabled(m_view->canRedo());
}

bool MainWindow::maybeSave()
{
    if (!m_view->isModified())
        return true;
    const auto r = QMessageBox::question(
        this, "Voltix", "Le schéma a été modifié. Voulez-vous l'enregistrer ?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (r == QMessageBox::Save)
        return saveFile();
    return r == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (maybeSave())
        e->accept();
    else
        e->ignore();
}

void MainWindow::newFile()
{
    if (!maybeSave())
        return;
    m_view->clearAll();
    m_path.clear();
    updateState();
}

bool MainWindow::openPath(const QString &path)
{
    if (!maybeSave())
        return false;
    if (!m_view->loadFromFile(path)) {
        QMessageBox::warning(this, "Voltix", "Impossible de lire ce fichier.");
        return false;
    }
    m_path = path;
    updateState();
    return true;
}

void MainWindow::openFile()
{
    const QString p = QFileDialog::getOpenFileName(this, "Ouvrir un schéma", QString(),
                                                   "Schéma Voltix (*.voltix)");
    if (!p.isEmpty())
        openPath(p);
}

bool MainWindow::saveFile()
{
    if (m_path.isEmpty())
        return saveFileAs();
    if (!m_view->saveToFile(m_path)) {
        QMessageBox::warning(this, "Voltix", "Impossible d'écrire le fichier.");
        return false;
    }
    updateState();
    return true;
}

bool MainWindow::saveFileAs()
{
    QString p = QFileDialog::getSaveFileName(this, "Enregistrer le schéma", QString(),
                                             "Schéma Voltix (*.voltix)");
    if (p.isEmpty())
        return false;
    if (!p.endsWith(".voltix"))
        p += ".voltix";
    const QString old = m_path;
    m_path = p;
    if (!saveFile()) {
        m_path = old;
        updateState();
        return false;
    }
    return true;
}