#pragma once

class QWidget;

namespace snapocr::platform::win32 {

// 把 Qt 窗口提到前台并激活。
// 热键/托盘唤起的后台进程常被 Windows 的前台锁拒绝（SetForegroundWindow 失效），
// 这里用 AttachThreadInput 临时附加到当前前台线程再设置，成功率更高。
void forceForegroundWindow(QWidget* widget);

}  // namespace snapocr::platform::win32
