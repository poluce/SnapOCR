#include "app/Application.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

#include <QApplication>
#include <QGuiApplication>
#include <QTextStream>

#include "snapocr/Version.h"
#include "ui/MainWindow.h"

namespace snapocr::app {
namespace {

constexpr auto kFlagHelp = "--help";
constexpr auto kFlagVersion = "--version";
constexpr auto kFlagSmokeTest = "--smoke-test";

void printLine(const QString& text)
{
    QTextStream stream(stdout);
    stream << text << Qt::endl;
}

}  // namespace

Application::Application(QObject* parent)
    : QObject(parent)
{
}

Application::~Application() = default;

int Application::run(const QStringList& arguments)
{
    const QStringList args = arguments.mid(1);

    if (args.contains(QString::fromLatin1(kFlagVersion)) || args.contains(QStringLiteral("-v"))) {
        printVersion();
        return EXIT_SUCCESS;
    }
    if (args.contains(QString::fromLatin1(kFlagHelp)) || args.contains(QStringLiteral("-h"))) {
        printHelp();
        return EXIT_SUCCESS;
    }
    if (args.contains(QString::fromLatin1(kFlagSmokeTest))) {
        return runSmokeTest();
    }

    m_mainWindow = std::make_unique<ui::MainWindow>();
    m_mainWindow->show();
    return QApplication::exec();
}

int Application::runSmokeTest()
{
    QTextStream out(stdout);
    out << "SnapOCR smoke test\n";
    out << "  version : " << snapocr::version::kVersion << '\n';

    const QString platformName = QGuiApplication::platformName();
    out << "  platform: " << platformName << '\n';
    if (platformName.isEmpty()) {
        out << "FAIL: Qt 平台插件未加载" << '\n';
        out.flush();
        return EXIT_FAILURE;
    }

    // 创建一次主窗口，验证 Qt Widgets 初始化链路（不进入事件循环、不显示）。
    const ui::MainWindow window;
    if (window.windowTitle().isEmpty()) {
        out << "FAIL: 主窗口初始化失败" << '\n';
        out.flush();
        return EXIT_FAILURE;
    }

    out << "PASS\n";
    out.flush();
    return EXIT_SUCCESS;
}

void Application::printVersion() const
{
    printLine(QStringLiteral("%1 %2")
                  .arg(QString::fromUtf8(snapocr::version::kAppName),
                       QString::fromUtf8(snapocr::version::kVersion)));
}

void Application::printHelp() const
{
    printLine(QStringLiteral("%1 %2")
                  .arg(QString::fromUtf8(snapocr::version::kAppName),
                       QString::fromUtf8(snapocr::version::kVersion)));
    printLine(QStringLiteral("用法: SnapOCR.exe [选项]"));
    printLine(QStringLiteral("选项:"));
    printLine(QStringLiteral("  -h, --help       显示帮助"));
    printLine(QStringLiteral("  -v, --version    显示版本"));
    printLine(QStringLiteral("  --smoke-test     启动自检（CI / 构建验证用）"));
}

}  // namespace snapocr::app
