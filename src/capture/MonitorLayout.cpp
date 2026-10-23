#include "capture/MonitorLayout.h"

#include <QGuiApplication>
#include <QHash>
#include <QScreen>
#include <QString>
#include <QtGlobal>

#include <algorithm>

#include <windows.h>

namespace snapocr::capture {
namespace {

struct RawMonitor
{
    QString deviceName;
    bool primary = false;
    QRect physicalGeometry;
};

BOOL CALLBACK monitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam)
{
    auto* raw = reinterpret_cast<QVector<RawMonitor>*>(lParam);
    if (raw == nullptr) {
        return TRUE;
    }
    MONITORINFOEXW info{};
    info.cbSize = sizeof(MONITORINFOEXW);
    if (::GetMonitorInfoW(hMonitor, &info) == FALSE) {
        return TRUE;
    }
    RawMonitor monitor;
    monitor.deviceName = QString::fromWCharArray(info.szDevice);
    monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    monitor.physicalGeometry = QRect(info.rcMonitor.left,
                                     info.rcMonitor.top,
                                     info.rcMonitor.right - info.rcMonitor.left,
                                     info.rcMonitor.bottom - info.rcMonitor.top);
    raw->append(monitor);
    return TRUE;
}

// Qt 的 QScreen::name() 在 Windows 上不保证是 \\.\DISPLAY1（可能是显示器型号名），
// 因此按“名字 → 主屏 → 相对位置排序”三级策略匹配；仍然落空时退化为 1:1。
QHash<QString, QScreen*> screensByName(const QList<QScreen*>& screens)
{
    QHash<QString, QScreen*> result;
    for (QScreen* screen : screens) {
        result.insert(screen->name(), screen);
    }
    return result;
}

void applyScreen(MonitorInfo* monitor, QScreen* screen)
{
    if (monitor == nullptr || screen == nullptr) {
        return;
    }
    monitor->displayName = screen->name();
    monitor->logicalGeometry = screen->geometry();

    // 用“物理宽 / 逻辑宽”反推有效缩放比：这样把物理像素图绘制到逻辑矩形时
    // 恰好落到物理像素上（1:1），也避免 Qt 报告的 dpr 与几何不一致。
    const int logicalWidth = screen->geometry().width();
    const int logicalHeight = screen->geometry().height();
    if (logicalWidth > 0 && logicalHeight > 0) {
        const qreal dprX = static_cast<qreal>(monitor->physicalGeometry.width()) / logicalWidth;
        const qreal dprY = static_cast<qreal>(monitor->physicalGeometry.height()) / logicalHeight;
        monitor->devicePixelRatio = (dprX + dprY) / 2.0;
    } else {
        monitor->devicePixelRatio = screen->devicePixelRatio();
    }
    if (monitor->devicePixelRatio <= 0.0) {
        monitor->devicePixelRatio = 1.0;
    }
}

}  // namespace

QVector<MonitorInfo> MonitorLayout::enumerate()
{
    QVector<RawMonitor> raw;
    ::EnumDisplayMonitors(nullptr, nullptr, monitorEnumProc, reinterpret_cast<LPARAM>(&raw));
    if (raw.isEmpty()) {
        return {};
    }

    const QList<QScreen*> screens = QGuiApplication::screens();
    const QHash<QString, QScreen*> byName = screensByName(screens);

    QVector<MonitorInfo> monitors;
    monitors.reserve(raw.size());
    for (const RawMonitor& item : raw) {
        MonitorInfo monitor;
        monitor.id = item.deviceName;
        monitor.displayName = item.deviceName;
        monitor.primary = item.primary;
        monitor.physicalGeometry = item.physicalGeometry;
        monitor.logicalGeometry = item.physicalGeometry;
        monitor.devicePixelRatio = 1.0;
        monitors.append(monitor);
    }

    QVector<bool> usedScreens(screens.size(), false);

    auto matchWith = [&](int monitorIndex, QScreen* screen) {
        if (screen == nullptr) {
            return false;
        }
        const int screenIndex = screens.indexOf(screen);
        if (screenIndex >= 0) {
            if (usedScreens[screenIndex]) {
                return false;
            }
            usedScreens[screenIndex] = true;
        }
        applyScreen(&monitors[monitorIndex], screen);
        return true;
    };

    // 1) 按名字精确匹配（Qt 可能给出设备名，也可能给出型号名）
    for (int i = 0; i < monitors.size(); ++i) {
        const auto it = byName.constFind(monitors[i].id);
        if (it != byName.constEnd()) {
            matchWith(i, it.value());
        }
    }

    // 2) 主屏匹配
    QScreen* primaryScreen = QGuiApplication::primaryScreen();
    for (int i = 0; i < monitors.size(); ++i) {
        if (!monitors[i].primary || monitors[i].displayName != monitors[i].id) {
            continue;
        }
        matchWith(i, primaryScreen);
    }

    // 3) 剩余按“相对主屏的方位”匹配：把未匹配的 Win32 显示器与 Qt 屏幕各自排序后配对
    QVector<int> unmatchedMonitors;
    for (int i = 0; i < monitors.size(); ++i) {
        if (monitors[i].displayName == monitors[i].id) {
            unmatchedMonitors.append(i);
        }
    }
    QVector<QScreen*> unmatchedScreens;
    for (int i = 0; i < screens.size(); ++i) {
        if (!usedScreens[i]) {
            unmatchedScreens.append(screens[i]);
        }
    }

    if (!unmatchedMonitors.isEmpty() && !unmatchedScreens.isEmpty()) {
        const QRect primaryPhysical = monitors.isEmpty() ? QRect() : monitors.first().physicalGeometry;
        std::sort(unmatchedMonitors.begin(), unmatchedMonitors.end(), [&](int a, int b) {
            const QRect ra = monitors[a].physicalGeometry;
            const QRect rb = monitors[b].physicalGeometry;
            if (ra.x() != rb.x()) {
                return ra.x() < rb.x();
            }
            return ra.y() < rb.y();
        });
        std::sort(unmatchedScreens.begin(), unmatchedScreens.end(),
                  [](QScreen* a, QScreen* b) {
                      const QPoint pa = a->geometry().topLeft();
                      const QPoint pb = b->geometry().topLeft();
                      if (pa.x() != pb.x()) {
                          return pa.x() < pb.x();
                      }
                      return pa.y() < pb.y();
                  });
        Q_UNUSED(primaryPhysical);

        const int count = std::min(unmatchedMonitors.size(), unmatchedScreens.size());
        for (int i = 0; i < count; ++i) {
            matchWith(unmatchedMonitors[i], unmatchedScreens[i]);
        }
    }

    return monitors;
}

QRect MonitorLayout::virtualPhysicalGeometry(const QVector<MonitorInfo>& monitors)
{
    if (monitors.isEmpty()) {
        return {};
    }
    QRect result = monitors.first().physicalGeometry;
    for (const MonitorInfo& monitor : monitors) {
        result = result.united(monitor.physicalGeometry);
    }
    return result;
}

}  // namespace snapocr::capture
