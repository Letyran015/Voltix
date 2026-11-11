#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "catalog.h"
#include "librarypanel.h"
#include "schemaview.h"
#include "updatechecker.h"
#include "welcomepage.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>

static const int kMods[3] = {ModElectronics, ModElectrotech, ModElectromag};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(1200, 760);

    auto *view = new SchemaView(this);
    m_view = view;
    m_welcome = new WelcomePage(this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(view);
    setCentralWidget(m_stack);

    // Bibliothèque (panneau de gauche)
    m_lib = new LibraryPanel;
    m_lib->setMinimumWidth(240);
    m_lib->onPick = [this, view](const QString &id) {
        view->setKind(id);
        statusBar()->showMessage(
            QString("%1 : double-cliquez dans le schéma pour le poser.").arg(entryName(id)));
    };
    m_dock = new QDockWidget("Bibliothèque", this);
    m_dock->setObjectName("library");
    m_dock->setWidget(m_lib);
    addDockWidget(Qt::LeftDockWidgetArea, m_dock);
    m_dock->hide();

    m_welcome->onChoose = [this](int m) { enterModule(m); };

    // Menu Fichier
    QMenu *fm = menuBar()->addMenu("&Fichier");
    fm->addAction("&Nouveau", QKeySequence::New, this, &MainWindow::newFile);
    fm->addAction("&Ouvrir…", QKeySequence::Open, this, &MainWindow::openFile);
    fm->addSeparator();
    fm->addAction("&Enregistrer", QKeySequence::Save, this, &MainWindow::saveFile);
    fm->addAction("Enregistrer &sous…", QKeySequence::SaveAs, this, &MainWindow::saveFileAs);
    fm->addSeparator();
    fm->addAction("Écran d'&accueil", this, [this] { showWelcome(); });

    // Menu Édition
    QMenu *em = menuBar()->addMenu("&Édition");
    m_undoAct = em->addAction("&Annuler", QKeySequence::Undo, this, [view] { view->undo(); });
    m_redoAct = em->addAction("&Rétablir", QKeySequence::Redo, this, [view] { view->redo(); });

    // Menu Affichage : module, bibliothèque, accueil
    QMenu *vm = menuBar()->addMenu("&Affichage");
    QMenu *mm = vm->addMenu("&Module");
    auto *mg = new QActionGroup(this);
    for (int i = 0; i < 3; ++i) {
        QAction *a = mm->addAction(moduleTitle(kMods[i]));
        a->setCheckable(true);
        mg->addAction(a);
        m_modActs << a;
        const int m = kMods[i];
        connect(a, &QAction::triggered, this, [this, m] { enterModule(m); });
    }
    QAction *dockAct = m_dock->toggleViewAction();
    dockAct->setText("Panneau &bibliothèque");
    vm->addAction(dockAct);
    vm->addSeparator();
    QAction *welcomeAct = vm->addAction("Afficher l'écran d'accueil au démarrage");
    welcomeAct->setCheckable(true);
    welcomeAct->setChecked(QSettings().value("ui/welcome", true).toBool());
    connect(welcomeAct, &QAction::toggled, this,
            [](bool on) { QSettings().setValue("ui/welcome", on); });

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

    // Menu Outils
    QMenu *tm = menuBar()->addMenu("&Outils");
    tm->addAction("&Vérifier le schéma électrique", QKeySequence(Qt::Key_F9), this,
                  [view] { view->verifyElectrical(); });

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

    // démarrage : écran d'accueil, ou directement le dernier module
    QSettings s;
    m_module = s.value("ui/module", ModElectronics).toInt();
    if (m_module != ModElectronics && m_module != ModElectrotech && m_module != ModElectromag)
        m_module = ModElectronics;
    if (s.value("ui/welcome", true).toBool())
        showWelcome();
    else
        enterModule(m_module);

    QTimer::singleShot(3000, this, [updater] { updater->check(false); });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::enterModule(int module)
{
    m_module = module;
    QSettings().setValue("ui/module", module);
    for (int i = 0; i < m_modActs.size() && i < 3; ++i)
        m_modActs[i]->setChecked(kMods[i] == module);
    m_stack->setCurrentWidget(m_view);
    m_dock->show();
    m_lib->setModule(module);
    m_view->setFocus();
}

void MainWindow::showWelcome()
{
    m_dock->hide();
    m_stack->setCurrentWidget(m_welcome);
    statusBar()->clearMessage();
    m_welcome->setFocus();
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
    if (m_stack->currentWidget() != m_view)       // ouvert depuis l'accueil : on passe à l'éditeur
        enterModule(m_module);
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