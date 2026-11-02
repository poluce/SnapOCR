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

BOOL CALLBACK monitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam)
{
    auto* raw = reinterpret_cast<QVector<detail::RawMonitorInfo>*>(lParam);
    if (raw == nullptr) {
        return TRUE;
    }
    MONITORINFOEXW info{};
    info.cbSize = sizeof(MONITORINFOEXW);
    if (::GetMonitorInfoW(hMonitor, &info) == FALSE) {
        return TRUE;
    }
    detail::RawMonitorInfo monitor;
    monitor.deviceName = QString::fromWCharArray(info.szDevice);
    monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    monitor.physicalGeometry = QRect(info.rcMonitor.left,
                                     info.rcMonitor.top,
                                     info.rcMonitor.right - info.rcMonitor.left,
                                     info.rcMonitor.bottom - info.rcMonitor.top);
    raw->append(monitor);
    return TRUE;
}

void applyScreen(MonitorInfo* monitor, const detail::ScreenInfo& screen)
{
    if (monitor == nullptr) {
        return;
    }
    monitor->displayName = screen.name;
    monitor->logicalGeometry = screen.logicalGeometry;

    // 用“物理宽 / 逻辑宽”反推有效缩放比：这样把物理像素图绘制到逻辑矩形时
    // 恰好落到物理像素上（1:1），也避免 Qt 报告的 dpr 与几何不一致。
    const int logicalWidth = screen.logicalGeometry.width();
    const int logicalHeight = screen.logicalGeometry.height();
    if (logicalWidth > 0 && logicalHeight > 0) {
        const qreal dprX = static_cast<qreal>(monitor->physicalGeometry.width()) / logicalWidth;
        const qreal dprY = static_cast<qreal>(monitor->physicalGeometry.height()) / logicalHeight;
        monitor->devicePixelRatio = (dprX + dprY) / 2.0;
    } else {
        monitor->devicePixelRatio = screen.devicePixelRatio;
    }
    if (monitor->devicePixelRatio <= 0.0) {
        monitor->devicePixelRatio = 1.0;
    }
}

}  // namespace

namespace detail {

QVector<int> matchScreens(const QVector<RawMonitorInfo>& monitors,
                          const QVector<ScreenInfo>& screens, int primaryScreenIndex)
{
    QVector<int> result(monitors.size(), -1);
    QVector<bool> usedScreens(screens.size(), false);

    auto match = [&](int monitorIndex, int screenIndex) {
        if (monitorIndex < 0 || monitorIndex >= monitors.size() || screenIndex < 0
            || screenIndex >= screens.size() || usedScreens[screenIndex]) {
            return false;
        }
        usedScreens[screenIndex] = true;
        result[monitorIndex] = screenIndex;
        return true;
    };

    // 1) 按名字精确匹配（Qt 可能给出设备名，也可能给出型号名）
    QHash<QString, int> byName;
    for (int i = 0; i < screens.size(); ++i) {
        byName.insert(screens[i].name, i);
    }
    for (int i = 0; i < monitors.size(); ++i) {
        const auto it = byName.constFind(monitors[i].deviceName);
        if (it != byName.constEnd()) {
            match(i, it.value());
        }
    }

    // 2) 主屏匹配：不能用 displayName == deviceName 判断“是否已匹配”，
    // 因为名字匹配成功时两者恰好相等，会被误当成未匹配并与副屏交换。
    for (int i = 0; i < monitors.size(); ++i) {
        if (result[i] >= 0 || !monitors[i].primary) {
            continue;
        }
        match(i, primaryScreenIndex);
    }

    // 3) 剩余按“相对主屏的方位”匹配：把未匹配的 Win32 显示器与 Qt 屏幕各自排序后配对
    QVector<int> unmatchedMonitors;
    for (int i = 0; i < monitors.size(); ++i) {
        if (result[i] < 0) {
            unmatchedMonitors.append(i);
        }
    }
    QVector<int> unmatchedScreens;
    for (int i = 0; i < screens.size(); ++i) {
        if (!usedScreens[i]) {
            unmatchedScreens.append(i);
        }
    }

    if (!unmatchedMonitors.isEmpty() && !unmatchedScreens.isEmpty()) {
        std::sort(unmatchedMonitors.begin(), unmatchedMonitors.end(), [&](int a, int b) {
            const QRect ra = monitors[a].physicalGeometry;
            const QRect rb = monitors[b].physicalGeometry;
            if (ra.x() != rb.x()) {
                return ra.x() < rb.x();
            }
            return ra.y() < rb.y();
        });
        std::sort(unmatchedScreens.begin(), unmatchedScreens.end(), [&](int a, int b) {
            const QPoint pa = screens[a].logicalGeometry.topLeft();
            const QPoint pb = screens[b].logicalGeometry.topLeft();
            if (pa.x() != pb.x()) {
                return pa.x() < pb.x();
            }
            return pa.y() < pb.y();
        });

        const int count = std::min(unmatchedMonitors.size(), unmatchedScreens.size());
        for (int i = 0; i < count; ++i) {
            match(unmatchedMonitors[i], unmatchedScreens[i]);
        }
    }

    return result;
}

}  // namespace detail

QVector<MonitorInfo> MonitorLayout::enumerate()
{
    QVector<detail::RawMonitorInfo> raw;
    ::EnumDisplayMonitors(nullptr, nullptr, monitorEnumProc, reinterpret_cast<LPARAM>(&raw));
    if (raw.isEmpty()) {
        return {};
    }

    const QList<QScreen*> screens = QGuiApplication::screens();
    QVector<detail::ScreenInfo> screenInfos;
    screenInfos.reserve(screens.size());
    for (const QScreen* screen : screens) {
        detail::ScreenInfo info;
        info.name = screen->name();
        info.logicalGeometry = screen->geometry();
        info.devicePixelRatio = screen->devicePixelRatio();
        screenInfos.append(info);
    }

    const int primaryScreenIndex = screens.indexOf(QGuiApplication::primaryScreen());
    const QVector<int> matches = detail::matchScreens(raw, screenInfos, primaryScreenIndex);

    QVector<MonitorInfo> monitors;
    monitors.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        MonitorInfo monitor;
        monitor.id = raw[i].deviceName;
        monitor.displayName = raw[i].deviceName;
        monitor.primary = raw[i].primary;
        monitor.physicalGeometry = raw[i].physicalGeometry;
        monitor.logicalGeometry = raw[i].physicalGeometry;
        monitor.devicePixelRatio = 1.0;
        if (i < matches.size() && matches[i] >= 0) {
            applyScreen(&monitor, screenInfos[matches[i]]);
        }
        monitors.append(monitor);
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
