#include <QApplication>
#include "mainwindow.h"
#include "logmanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // LogManager::instance() 的调用将自动安装消息处理函数
    LogManager::instance();

    MainWindow w;
    w.show();

    return a.exec();
}
