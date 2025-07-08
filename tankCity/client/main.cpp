#include "loadingwidget.h"
#include "loginwindow.h"
#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qDebug() << "Application started";
    // 创建加载窗口
    LoadingWidget *loader = new LoadingWidget();
    loader->show();

    // 创建登录窗口指针
    LoginWindow *loginWindow = nullptr;

    QObject::connect(loader, &LoadingWidget::loadingFinished, [&]() {
        loginWindow = new LoginWindow();
        loginWindow->show();
        loader->deleteLater();
    });

    return a.exec();
}
