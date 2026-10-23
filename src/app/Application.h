#pragma once

#include <QObject>
#include <QStringList>

#include <memory>

namespace snapocr::ui {
class MainWindow;
}

namespace snapocr::app {

// 应用控制器：解析命令行、组织主窗口与后续的托盘/热键/服务装配。
// 生命周期内不拥有 QApplication；由 main() 创建并传入参数。
class Application final : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    // 处理命令行参数并进入事件循环，返回进程退出码。
    int run(const QStringList& arguments);

private:
    int runSmokeTest();
    void printVersion() const;
    void printHelp() const;

    std::unique_ptr<ui::MainWindow> m_mainWindow;
};

}  // namespace snapocr::app
