#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "core.h"
#include <QDebug>
#include "schemaview.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setCentralWidget(new SchemaView(this));
    setWindowTitle("Voltix");
}

MainWindow::~MainWindow()
{
    delete ui;
}
