#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class SchemaView;
class QAction;
class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    Ui::MainWindow *ui;
    SchemaView *m_view = nullptr;
    QString m_path;
    QAction *m_undoAct = nullptr;
    QAction *m_redoAct = nullptr;

    void updateState();
    bool maybeSave();
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
};
#endif // MAINWINDOW_H