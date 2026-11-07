#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "core.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qDebug() << "core_add(2, 3) =" << core_add(2, 3);
}

MainWindow::~MainWindow()
{
    delete ui;
}
