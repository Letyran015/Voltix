#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class SchemaView;
class WelcomePage;
class LibraryPanel;
class QStackedWidget;
class QDockWidget;
class QAction;
class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    bool openPath(const QString &path);     // ouvre un fichier .voltix

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    Ui::MainWindow *ui;
    SchemaView *m_view = nullptr;
    WelcomePage *m_welcome = nullptr;
    LibraryPanel *m_lib = nullptr;
    QStackedWidget *m_stack = nullptr;
    QDockWidget *m_dock = nullptr;
    QString m_path;
    int m_module = 1;
    QAction *m_undoAct = nullptr;
    QAction *m_redoAct = nullptr;
    QList<QAction *> m_modActs;

    void enterModule(int module);
    void showWelcome();
    void updateState();
    bool maybeSave();
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
};
#endif // MAINWINDOW_H