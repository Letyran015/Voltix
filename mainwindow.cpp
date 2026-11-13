#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "bench.h"
#include "catalog.h"
#include "librarypanel.h"
#include "projectdialog.h"
#include "schemaview.h"
#include "updatechecker.h"
#include "welcomepage.h"
#include "wireitem.h"


#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>


// Banc virtuel : fenêtre indépendante du schéma, détruite à sa fermeture (WA_DeleteOnClose)
static void openBench()
{
    auto *bench = new BenchWindow;
    bench->show();
    bench->raise();
    bench->activateWindow();
}

static const int kMods[3] = {ModElectronics, ModElectrotech, ModElectromag};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(1280, 800);

    auto *view = new SchemaView(this);
    m_view = view;
    m_welcome = new WelcomePage(this);

    // éditeur = zone de dessin + onglets de folios en bas
    m_editor = new QWidget(this);
    auto *vl = new QVBoxLayout(m_editor);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    vl->addWidget(view, 1);
    m_tabs = new QTabBar;
    m_tabs->setShape(QTabBar::RoundedSouth);
    m_tabs->setExpanding(false);
    vl->addWidget(m_tabs);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_editor);
    setCentralWidget(m_stack);

    // Bibliothèque (panneau de gauche)
    m_lib = new LibraryPanel;
    m_lib->setMinimumWidth(250);
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

    // Barre « Fils » : nature et section des prochains fils tracés
    m_wireBar = addToolBar("Fils");
    m_wireBar->setObjectName("wires");
    m_wireBar->addWidget(new QLabel(" Fil : "));
    auto *nat = new QComboBox;
    for (const WireNature &n : wireNatures())
        nat->addItem(QString::fromUtf8(n.label), QString::fromLatin1(n.id));
    m_wireBar->addWidget(nat);
    m_wireBar->addWidget(new QLabel("  Section : "));
    auto *sec = new QComboBox;
    sec->setEditable(true);
    sec->addItems({"", "0.5", "0.75", "1", "1.5", "2.5", "4", "6", "10", "16", "25", "35", "50"});
    sec->setMinimumWidth(80);
    m_wireBar->addWidget(sec);
    m_wireBar->addWidget(new QLabel(" mm² "));
    connect(nat, &QComboBox::currentIndexChanged, this,
            [view, nat] { view->setWireNature(nat->currentData().toString()); });
    connect(sec, &QComboBox::currentTextChanged, this,
            [view](const QString &t) { view->setWireSection(QString(t).trimmed().replace(',', '.')); });
    m_wireBar->hide();

    m_welcome->onChoose = [this](int m) { enterModule(m); };

    // onglets de folios
    connect(m_tabs, &QTabBar::currentChanged, this, [this](int i) {
        if (i >= 0 && i != m_view->currentFolio())
            m_view->setFolio(i);
    });
    connect(m_tabs, &QTabBar::tabBarDoubleClicked, this, [this](int i) {
        if (i >= 0)
            renameFolio(i);
    });
    connect(view, &SchemaView::foliosChanged, this, &MainWindow::rebuildTabs);

    // Menu Fichier
    QMenu *fm = menuBar()->addMenu("&Fichier");
    fm->addAction("&Nouveau", QKeySequence::New, this, &MainWindow::newFile);
    fm->addAction("&Ouvrir…", QKeySequence::Open, this, &MainWindow::openFile);
    fm->addSeparator();
    fm->addAction("&Enregistrer", QKeySequence::Save, this, &MainWindow::saveFile);
    fm->addAction("Enregistrer &sous…", QKeySequence::SaveAs, this, &MainWindow::saveFileAs);
    fm->addAction("E&xporter en PDF…", QKeySequence("Ctrl+P"), this, &MainWindow::exportPdf);
    fm->addSeparator();
    fm->addAction("Écran d'&accueil", this, [this] { showWelcome(); });

    // Menu Édition
    QMenu *em = menuBar()->addMenu("&Édition");
    m_undoAct = em->addAction("&Annuler", QKeySequence::Undo, this, [view] { view->undo(); });
    m_redoAct = em->addAction("&Rétablir", QKeySequence::Redo, this, [view] { view->redo(); });

    // Menu Folio (visible en électrotechnique seulement)
    QMenu *fo = menuBar()->addMenu("F&olio");
    m_folioMenu = fo;
    fo->addAction("&Nouveau folio", QKeySequence("Ctrl+Shift+N"), this, [view] { view->addFolio(); });
    fo->addAction("&Renommer le folio…", this, [this] { renameFolio(m_view->currentFolio()); });
    fo->addAction("&Supprimer le folio", this, [this] {
        if (m_view->folioCount() <= 1) {
            QMessageBox::information(this, "Voltix", "Le projet doit garder au moins un folio.");
            return;
        }
        if (QMessageBox::question(this, "Voltix", "Supprimer définitivement ce folio ?")
            == QMessageBox::Yes)
            m_view->removeFolio();
    });
    fo->addSeparator();
    fo->addAction("Folio &précédent", QKeySequence("Ctrl+PgUp"), this,
                  [this] { m_view->setFolio(m_view->currentFolio() - 1); });
    fo->addAction("Folio &suivant", QKeySequence("Ctrl+PgDown"), this,
                  [this] { m_view->setFolio(m_view->currentFolio() + 1); });
    fo->addSeparator();
    fo->addAction("&Cartouche et format de feuille…", this, [this] { editProject(); });

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
    vm->addAction("&Ajuster à la feuille", QKeySequence("Ctrl+0"), this, [view] { view->zoomFit(); });
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
    sm->addSeparator();
    sm->addAction("Banc &virtuel d'électrotechnique…", QKeySequence("Ctrl+B"), this,
                  [] { openBench(); });

    // Menu Outils
    QMenu *tm = menuBar()->addMenu("&Outils");
    tm->addAction("&Vérifier le projet", QKeySequence(Qt::Key_F9), this,
                  [view] { view->verifyElectrical(); });
    tm->addSeparator();
    tm->addAction("Banc virtuel d'électrotechnique…", this, [] { openBench(); });

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
    rebuildTabs();
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

    const bool sheet = (module == ModElectrotech);     // folios et cartouche : électrotechnique seulement
    m_view->setSheetEnabled(sheet);
    m_tabs->setVisible(sheet);
    m_folioMenu->menuAction()->setVisible(sheet);
    m_wireBar->setVisible(sheet);

    menuBar()->setVisible(true);
    m_stack->setCurrentWidget(m_editor);
    m_dock->show();
    m_lib->setModule(module);
    m_view->setFocus();
}

void MainWindow::showWelcome()
{
    m_dock->hide();
    m_wireBar->hide();
    menuBar()->setVisible(false);                      // écran d'accueil sans la bande de menus
    m_stack->setCurrentWidget(m_welcome);
    statusBar()->clearMessage();
    m_welcome->setFocus();
}

void MainWindow::rebuildTabs()
{
    QSignalBlocker block(m_tabs);
    while (m_tabs->count() > 0)
        m_tabs->removeTab(0);
    for (int i = 0; i < m_view->folioCount(); ++i)
        m_tabs->addTab(QString("%1 · %2").arg(i + 1, 2, 10, QChar('0')).arg(m_view->folioTitle(i)));
    m_tabs->setCurrentIndex(m_view->currentFolio());
}

void MainWindow::renameFolio(int i)
{
    bool ok = false;
    const QString t = QInputDialog::getText(this, "Renommer le folio", "Titre du folio :",
                                            QLineEdit::Normal, m_view->folioTitle(i), &ok);
    if (ok)
        m_view->renameFolio(i, t);
}

void MainWindow::editProject()
{
    ProjectDialog dlg(m_view->project(), this);
    if (dlg.exec() == QDialog::Accepted)
        m_view->setProject(dlg.info());
}

void MainWindow::exportPdf()
{
    QString base = m_path.isEmpty() ? QString("schema") : QFileInfo(m_path).completeBaseName();
    QString p = QFileDialog::getSaveFileName(this, "Exporter en PDF", base + ".pdf", "PDF (*.pdf)");
    if (p.isEmpty())
        return;
    if (!p.endsWith(".pdf", Qt::CaseInsensitive))
        p += ".pdf";
    if (m_view->exportPdf(p))
        statusBar()->showMessage("PDF exporté : " + p, 6000);
    else
        QMessageBox::warning(this, "Voltix", "Impossible d'écrire le PDF.");
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
        this, "Voltix", "Le projet a été modifié. Voulez-vous l'enregistrer ?",
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
    if (m_stack->currentWidget() != m_editor)
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
