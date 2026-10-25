#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QSize>
#include <QString>

class QPainter;

namespace snapocr::overlay {

// 像素放大镜 + 取色的绘制助手（F-CAP-14）。
//
// 不做成独立控件：覆盖层只有在拖拽（鼠标捕获）时才会收到移动事件，
// 独立子控件还额外受父窗口绘制顺序影响。这里只提供纯函数，
// 由 OverlayWindow 在自己的 paintEvent 里画，位置与内容完全可控。
class ColorMagnifier
{
public:
    static constexpr int kGridPixels = 11;  // 11x11 取样窗口
    static constexpr int kCellSize = 10;    // 每格逻辑像素
    static constexpr int kInfoHeight = 38;
    static constexpr int kPadding = 6;
    static constexpr int kCursorGap = 18;

    // 取图像某点的颜色（越界夹取到边缘；空图返回黑色）。
    static QColor sampleColor(const QImage& image, const QPoint& point);
    static QString hexText(const QColor& color);
    static QString rgbText(const QColor& color);

    static QSize panelSize();
    // 面板左上角位置：优先光标右下，越界时翻到另一侧并夹取进父区域。
    static QRectF panelRect(const QPoint& localLogicalCursor, const QSize& parentSize);

    // 在指定区域内绘制放大镜面板。
    static void paint(QPainter& painter, const QRectF& panel, const QImage& frameImage,
                      const QPoint& samplePoint);
};

}  // namespace snapocr::overlay
