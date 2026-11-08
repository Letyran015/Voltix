#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
class SchemaView;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    SchemaView *m_view = nullptr;
    QString m_path;
    void updateTitle();
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
};
#endif // MAINWINDOW_H
