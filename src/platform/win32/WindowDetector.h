#pragma once

#include <QRect>
#include <QVector>
#include <QString>

namespace snapocr::platform::win32 {

// 光标下的窗口（F-CAP-15）。
struct WindowTarget
{
    quintptr handle = 0;
    QRect physicalGeometry;  // 虚拟桌面物理像素坐标（已裁剪到屏幕内）
    QString title;
    QString className;
};

// 查询光标位置下的顶层窗口。
// 过滤：自身进程窗口、不可见/最小化窗口、DWM cloak 窗口、桌面壳窗口、
// 尺寸过小或覆盖整个虚拟桌面的窗口。
// 返回 false 表示该点没有可截取的窗口。
bool windowAtPhysicalPoint(const QPoint& physicalPoint, WindowTarget* target);

// 诊断/测试用：按 Z 序返回包含该点、且通过过滤规则的窗口（最多 maxCount 个）。
// includeOwnProcess=true 时把自己进程的窗口也纳入（测试里验证枚举逻辑用）。
QVector<WindowTarget> enumerateWindowsAt(const QPoint& physicalPoint, int maxCount = 8,
                                        bool includeOwnProcess = false);

// 纯函数（可单测）：窗口矩形是否适合作为截图目标。
// virtualDesktop 为虚拟桌面物理矩形；太小（< 8×8）或覆盖整个桌面的矩形会被拒绝。
bool isSnippingTargetRectValid(const QRect& physicalRect, const QRect& virtualDesktop);

// 纯函数（可单测）：把矩形裁剪进虚拟桌面；完全在桌面外时返回空矩形。
QRect clampToVirtualDesktop(const QRect& physicalRect, const QRect& virtualDesktop);

}  // namespace snapocr::platform::win32
