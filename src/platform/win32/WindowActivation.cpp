#include "platform/win32/WindowActivation.h"

#include <QWidget>

#include <windows.h>

namespace snapocr::platform::win32 {

void forceForegroundWindow(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }
    widget->raise();
    widget->activateWindow();

    auto* hwnd = reinterpret_cast<HWND>(widget->winId());
    if (hwnd == nullptr) {
        return;
    }

    if (::GetForegroundWindow() == hwnd) {
        return;
    }

    // 不使用 AttachThreadInput：它会临时共享输入队列，导致鼠标坐标/激活时序异常
    // （实测出现“按下坐标与真实光标位置不一致”的偶发问题）。
    // 只用标准方式尝试前台化；系统拒绝时也不影响用户正常点击（点击本身会激活窗口）。
    ::BringWindowToTop(hwnd);
    ::SetForegroundWindow(hwnd);
    ::SetActiveWindow(hwnd);
    ::SetFocus(hwnd);
}

}  // namespace snapocr::platform::win32
