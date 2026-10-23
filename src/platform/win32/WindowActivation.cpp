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

    const DWORD foregroundThread = ::GetWindowThreadProcessId(::GetForegroundWindow(), nullptr);
    const DWORD currentThread = ::GetCurrentThreadId();
    bool attached = false;
    if (foregroundThread != 0 && foregroundThread != currentThread) {
        attached = ::AttachThreadInput(currentThread, foregroundThread, TRUE) != FALSE;
    }

    ::BringWindowToTop(hwnd);
    ::SetForegroundWindow(hwnd);
    ::SetActiveWindow(hwnd);
    ::SetFocus(hwnd);

    if (attached) {
        ::AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
}

}  // namespace snapocr::platform::win32
