#include "platform/win32/WindowDetector.h"

#include <QDebug>
#include <QVector>

#include <windows.h>
#include <dwmapi.h>

namespace snapocr::platform::win32 {
namespace {

constexpr int kMinTargetSide = 8;

// 这些类名属于桌面/任务栏/开始菜单等壳窗口，不应作为截图目标。
bool isShellWindowClass(const QString& className)
{
    static const QVector<QString> blocked = {
        QStringLiteral("Progman"),          // 桌面
        QStringLiteral("WorkerW"),         // 桌面壁纸层
        QStringLiteral("Shell_TrayWnd"),   // 任务栏
        QStringLiteral("Shell_SecondaryTrayWnd"),
        QStringLiteral("NotifyIconOverflowWindow"),
        QStringLiteral("Windows.UI.Core.CoreWindow"),  // 开始菜单 / 系统弹层
        QStringLiteral("XamlExplorerHostIslandWindow"),
        QStringLiteral("ApplicationFrameWindow"),  // UWP 壳（内部窗口仍可命中）
    };
    return blocked.contains(className);
}

QString windowClassName(HWND window)
{
    wchar_t buffer[256] = {};
    const int length = ::GetClassNameW(window, buffer, 256);
    return QString::fromWCharArray(buffer, length > 0 ? length : 0);
}

QString windowTitle(HWND window)
{
    wchar_t buffer[512] = {};
    const int length = ::GetWindowTextW(window, buffer, 512);
    return QString::fromWCharArray(buffer, length > 0 ? length : 0);
}

bool isCloaked(HWND window)
{
    DWORD cloaked = 0;
    if (SUCCEEDED(::DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
        return cloaked != 0;
    }
    return false;
}

// 优先用 DWM 的可见边框（不含不可见的缩放边框），失败时退回 GetWindowRect。
QRect windowFrame(HWND window)
{
    RECT rect{};
    if (SUCCEEDED(::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &rect,
                                          sizeof(rect)))) {
        return QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    }
    if (::GetWindowRect(window, &rect) != FALSE) {
        return QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    }
    return {};
}

QRect virtualDesktopPhysical()
{
    const QRect virtualRect(::GetSystemMetrics(SM_XVIRTUALSCREEN), ::GetSystemMetrics(SM_YVIRTUALSCREEN),
                            ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
                            ::GetSystemMetrics(SM_CYVIRTUALSCREEN));
    if (virtualRect.isEmpty()) {
        return QRect(0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
    }
    return virtualRect;
}

// 覆盖层自己是最顶层窗口，WindowFromPoint 只会命中自己；
// 因此按 Z 序从上到下枚举顶层窗口，跳过本进程/壳窗口，取第一个包含该点的窗口。
HWND topLevelWindowAt(const QPoint& physicalPoint, const QRect& virtualDesktop,
                      bool includeOwnProcess = false)
{
    const POINT point{physicalPoint.x(), physicalPoint.y()};
    for (HWND window = ::GetTopWindow(nullptr); window != nullptr;
         window = ::GetWindow(window, GW_HWNDNEXT)) {
        if (::IsWindowVisible(window) == FALSE || ::IsIconic(window) != FALSE) {
            continue;
        }
        DWORD processId = 0;
        ::GetWindowThreadProcessId(window, &processId);
        if (!includeOwnProcess && processId == ::GetCurrentProcessId()) {
            continue;  // 自己的覆盖层/工具条/贴图窗
        }
        const QString className = windowClassName(window);
        if (isShellWindowClass(className) || isCloaked(window)) {
            continue;
        }
        const QRect frame = clampToVirtualDesktop(windowFrame(window), virtualDesktop);
        if (!isSnippingTargetRectValid(frame, virtualDesktop)) {
            continue;
        }
        if (!frame.contains(physicalPoint)) {
            continue;
        }
        // 顶层窗口的客户区可能比窗口小，但截图按窗口边框取整块，符合直觉。
        Q_UNUSED(point);
        return window;
    }
    return nullptr;
}

}  // namespace

QRect clampToVirtualDesktop(const QRect& physicalRect, const QRect& virtualDesktop)
{
    if (physicalRect.isEmpty() || virtualDesktop.isEmpty()) {
        return {};
    }
    return physicalRect.intersected(virtualDesktop);
}

bool isSnippingTargetRectValid(const QRect& physicalRect, const QRect& virtualDesktop)
{
    if (physicalRect.width() < kMinTargetSide || physicalRect.height() < kMinTargetSide) {
        return false;
    }
    if (virtualDesktop.isEmpty()) {
        return true;
    }
    // 覆盖整个虚拟桌面 = 桌面本身，不做窗口吸附
    if (physicalRect.contains(virtualDesktop) || physicalRect == virtualDesktop) {
        return false;
    }
    return true;
}

QVector<WindowTarget> enumerateWindowsAt(const QPoint& physicalPoint, int maxCount,
                                        bool includeOwnProcess)
{
    QVector<WindowTarget> result;
    const QRect desktop = virtualDesktopPhysical();
    for (HWND window = ::GetTopWindow(nullptr); window != nullptr && result.size() < maxCount;
         window = ::GetWindow(window, GW_HWNDNEXT)) {
        if (::IsWindowVisible(window) == FALSE || ::IsIconic(window) != FALSE) {
            continue;
        }
        DWORD processId = 0;
        ::GetWindowThreadProcessId(window, &processId);
        if (!includeOwnProcess && processId == ::GetCurrentProcessId()) {
            continue;
        }
        const QString className = windowClassName(window);
        if (isShellWindowClass(className) || isCloaked(window)) {
            continue;
        }
        const QRect frame = clampToVirtualDesktop(windowFrame(window), desktop);
        if (!isSnippingTargetRectValid(frame, desktop) || !frame.contains(physicalPoint)) {
            continue;
        }
        WindowTarget target;
        target.handle = reinterpret_cast<quintptr>(window);
        target.physicalGeometry = frame;
        target.title = windowTitle(window);
        target.className = className;
        result.append(target);
    }
    return result;
}

bool windowAtPhysicalPoint(const QPoint& physicalPoint, WindowTarget* target)
{
    if (target == nullptr) {
        return false;
    }
    const QRect desktop = virtualDesktopPhysical();
    HWND window = topLevelWindowAt(physicalPoint, desktop);
    if (window == nullptr) {
        return false;
    }

    const QString className = windowClassName(window);
    const QRect frame = clampToVirtualDesktop(windowFrame(window), desktop);
    if (!isSnippingTargetRectValid(frame, desktop)) {
        return false;
    }

    target->handle = reinterpret_cast<quintptr>(window);
    target->physicalGeometry = frame;
    target->title = windowTitle(window);
    target->className = className;
    return true;
}

}  // namespace snapocr::platform::win32
