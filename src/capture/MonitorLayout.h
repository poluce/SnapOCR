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

}  // namespace snapocr::capture
