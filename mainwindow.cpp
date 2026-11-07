#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "core.h"
#include <QDebug>
#include "schemaview.h"
#include <QActionGroup>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    auto *view = new SchemaView(this);
    setCentralWidget(view);

    auto *tb = addToolBar("Composants");
    auto *group = new QActionGroup(this);
    const QStringList names = {"Résistance", "Condensateur", "Diode", "Source de tension", "Masse"};
    const QStringList ids   = {"resistor", "capacitor", "diode", "vsource", "ground"};
    for (int i = 0; i < names.size(); ++i) {
        QAction *a = tb->addAction(names[i]);
        a->setCheckable(true);
        group->addAction(a);
        if (i == 0) a->setChecked(true);
        const QString id = ids[i];
        connect(a, &QAction::triggered, this, [view, id] { view->setKind(id); });
    }
    setWindowTitle("Voltix");
}

MainWindow::~MainWindow()
{
    delete ui;
}
