#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

namespace snapocr::capture {

// 一块显示器的信息。物理几何是权威坐标（虚拟桌面物理像素，可为负），
// 逻辑几何仅用于 Qt 覆盖层窗口定位与 UI 绘制。
struct MonitorInfo
{
    QString id;              // 设备名，如 \\.\DISPLAY1
    QString displayName;     // 更友好的名字（Qt screen name）
    bool primary = false;
    QRect physicalGeometry;  // 物理像素，虚拟桌面坐标
    QRect logicalGeometry;   // Qt 逻辑坐标
    qreal devicePixelRatio = 1.0;
    int rotationDegrees = 0;
};

struct ScreenFrame
{
    MonitorInfo monitor;
    QImage image;            // 物理像素，Format_RGB32
};

struct CaptureResult
{
    QVector<ScreenFrame> screens;
    QRect virtualGeometry;   // 所有显示器物理像素并集
    qint64 capturedAtMs = 0;

    bool isValid() const { return !screens.isEmpty(); }

    const ScreenFrame* frameForScreen(const QString& screenId) const
    {
        for (const ScreenFrame& frame : screens) {
            if (frame.monitor.id == screenId) {
                return &frame;
            }
        }
        return nullptr;
    }
};

}  // namespace snapocr::capture
