#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QStringList>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("Voltix");
    QCoreApplication::setApplicationName("Voltix");

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "Voltix_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    MainWindow w;
    w.show();

    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1 && args[1].endsWith(".voltix", Qt::CaseInsensitive))
        w.openPath(args[1]);

    return a.exec();
}