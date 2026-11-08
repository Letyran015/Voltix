#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "schemaview.h"

#include <QActionGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStringList>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto *view = new SchemaView(this);
    m_view = view;
    setCentralWidget(view);
    updateTitle();

    // Barre d'outils : choix du composant posé au double-clic
    auto *tb = addToolBar("Composants");
    auto *group = new QActionGroup(this);
    const QStringList names = {"Résistance", "Condensateur", "Diode", "Source de tension", "Masse"};
    const QStringList ids   = {"resistor", "capacitor", "diode", "vsource", "ground"};
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
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTitle()
{
    const QString name = m_path.isEmpty() ? QString("Sans titre") : QFileInfo(m_path).fileName();
    setWindowTitle("Voltix - " + name);
}

void MainWindow::newFile()
{
    m_view->clearAll();
    m_path.clear();
    updateTitle();
}

void MainWindow::openFile()
{
    const QString p = QFileDialog::getOpenFileName(this, "Ouvrir un schéma", QString(),
                                                   "Schéma Voltix (*.voltix)");
    if (p.isEmpty())
        return;
    if (!m_view->loadFromFile(p)) {
        QMessageBox::warning(this, "Voltix", "Impossible de lire ce fichier.");
        return;
    }
    m_path = p;
    updateTitle();
}

bool MainWindow::saveFile()
{
    if (m_path.isEmpty())
        return saveFileAs();
    if (!m_view->saveToFile(m_path)) {
        QMessageBox::warning(this, "Voltix", "Impossible d'écrire le fichier.");
        return false;
    }
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
    m_path = p;
    updateTitle();
    return saveFile();
}
