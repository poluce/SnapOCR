#include "capture/CaptureCrop.h"

#include <QPainter>

namespace snapocr::capture {

QImage cropSelection(const CaptureResult& result, const QRect& physicalRect)
{
    if (physicalRect.isEmpty() || result.screens.isEmpty()) {
        return {};
    }

    QImage output(physicalRect.size(), QImage::Format_ARGB32);
    output.fill(Qt::black);

    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (const ScreenFrame& frame : result.screens) {
        const QRect screenRect = frame.monitor.physicalGeometry;
        const QRect intersection = screenRect.intersected(physicalRect);
        if (intersection.isEmpty()) {
            continue;
        }
        // 该屏图像中的源矩形（相对于屏幕左上角）
        const QRect sourceRect(intersection.x() - screenRect.x(),
                               intersection.y() - screenRect.y(),
                               intersection.width(),
                               intersection.height());
        // 目标位置（相对于选区左上角）
        const QRect targetRect(intersection.x() - physicalRect.x(),
                               intersection.y() - physicalRect.y(),
                               intersection.width(),
                               intersection.height());
        painter.drawImage(targetRect, frame.image, sourceRect);
    }
    painter.end();
    return output;
}

}  // namespace snapocr::capture
