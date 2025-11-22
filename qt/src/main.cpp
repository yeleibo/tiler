#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置应用程序信息
    QApplication::setApplicationName("MapCloud Tiler");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("MapCloud");

    // 加载中文翻译（如果需要）
    QTranslator translator;
    if (translator.load(QLocale(), "tiler", "_", ":/translations")) {
        app.installTranslator(&translator);
    }

    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
