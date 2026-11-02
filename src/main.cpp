#include "mainwindow.h"
#include "theme.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char** argv) {
    QApplication a(argc, argv);
    QApplication::setApplicationName("H3Hint");
    QApplication::setWindowIcon(QIcon(":/appicon.png"));
    Theme::apply(&a);
    MainWindow w;
    w.show();
    return a.exec();
}
