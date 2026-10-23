#include <QApplication>
#include <QCoreApplication>
#include <QString>

#include "app/Application.h"
#include "platform/win32/Console.h"
#include "snapocr/Version.h"

int main(int argc, char* argv[])
{
    // WIN32 GUI 子系统进程默认没有控制台；从命令行启动时挂接父控制台，
    // 保证 --version / --help / --smoke-test 有可读输出。
    snapocr::platform::win32::attachParentConsole();

    QApplication application(argc, argv);
    application.setApplicationName(QString::fromUtf8(snapocr::version::kAppName));
    application.setApplicationVersion(QString::fromUtf8(snapocr::version::kVersion));
    application.setOrganizationName(QString::fromUtf8(snapocr::version::kOrganizationName));

    snapocr::app::Application controller;
    return controller.run(QCoreApplication::arguments());
}
