#pragma once

#include <QIcon>

namespace snapocr::ui {

// 应用图标（程序化绘制，避免外部资源与许可证问题）。
QIcon appIcon();
// 托盘图标（浅色/深色任务栏通用）。
QIcon trayIcon();

}  // namespace snapocr::ui
