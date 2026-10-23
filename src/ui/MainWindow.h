#pragma once

#include <QMainWindow>

namespace snapocr::ui {

// M0 占位主窗口：仅用于验证 UI 框架与构建链路。
// 后续阶段将由托盘 + 截图覆盖层取代，主窗口只承担设置/关于等入口。
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace snapocr::ui
