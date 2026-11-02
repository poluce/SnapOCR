#pragma once

#include <QRect>
#include <QVector>

#include "capture/CaptureTypes.h"

namespace snapocr::capture {

// 显示器枚举：Win32 物理几何 + Qt 逻辑几何/DPR。
class MonitorLayout
{
public:
    // 需在 QGuiApplication 创建后调用；失败返回空列表。
    static QVector<MonitorInfo> enumerate();

    static QRect virtualPhysicalGeometry(const QVector<MonitorInfo>& monitors);
};

namespace detail {

// Win32 显示器（EnumDisplayMonitors 原始数据）与 Qt 屏幕的纯数据快照，
// 便于脱离真实多屏环境对“配对”逻辑做单测。
struct RawMonitorInfo
{
    QString deviceName;
    QRect physicalGeometry;
    bool primary = false;
};

struct ScreenInfo
{
    QString name;
    QRect logicalGeometry;
    qreal devicePixelRatio = 1.0;
};

// 把 Win32 显示器与 Qt 屏幕配对，返回与 monitors 等长的屏幕下标（-1 = 未匹配）。
// 规则：设备名精确匹配 → 主屏匹配主显示器 → 剩余按相对方位排序配对。
QVector<int> matchScreens(const QVector<RawMonitorInfo>& monitors,
                          const QVector<ScreenInfo>& screens, int primaryScreenIndex);

}  // namespace detail

}  // namespace snapocr::capture
