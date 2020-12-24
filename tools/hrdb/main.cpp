#include "mainwindow.h"

#include <QApplication>
#include <QProcess>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("Hatari team");
    QCoreApplication::setApplicationName("hrdb");
    QApplication a(argc, argv);

    //QProcess proc;
    //QStringList args;
    //args.append("--debug");
    //proc.setProgram("hatari");
    //proc.setArguments(args);
    //proc.startDetached();

    MainWindow w;
    w.show();
    return a.exec();
}
