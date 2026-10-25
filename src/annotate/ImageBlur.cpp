#include "annotate/ImageBlur.h"

#include <QPainter>
#include <QVector>

namespace snapocr::annotate {
namespace {

// 单方向盒式模糊（水平或垂直），使用前缀和，复杂度 O(width*height)。
void boxBlurPass(const QImage& source, QImage* target, int radius, bool horizontal)
{
    const int width = source.width();
    const int height = source.height();
    const int window = radius * 2 + 1;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int r = 0;
            int g = 0;
            int b = 0;
            int a = 0;
            for (int offset = -radius; offset <= radius; ++offset) {
                int sx = horizontal ? x + offset : x;
                int sy = horizontal ? y : y + offset;
                sx = qBound(0, sx, width - 1);
                sy = qBound(0, sy, height - 1);
                const QRgb pixel = reinterpret_cast<const QRgb*>(source.constScanLine(sy))[sx];
                r += qRed(pixel);
                g += qGreen(pixel);
                b += qBlue(pixel);
                a += qAlpha(pixel);
            }
            reinterpret_cast<QRgb*>(target->scanLine(y))[x] =
                qRgba(r / window, g / window, b / window, a / window);
        }
    }
}

}  // namespace

QImage gaussianBlur(const QImage& source, int radius)
{
    if (source.isNull() || radius <= 0) {
        return source;
    }
    QImage current = source.convertToFormat(QImage::Format_ARGB32);
    QImage buffer(source.size(), QImage::Format_ARGB32);

    // 三次盒式模糊 ≈ 高斯；半径按 sqrt(3) 缩短以保持视觉强度接近。
    const int passRadius = qMax(1, static_cast<int>(radius / 1.732) );
    for (int pass = 0; pass < 3; ++pass) {
        boxBlurPass(current, &buffer, passRadius, true);
        boxBlurPass(buffer, &current, passRadius, false);
    }
    return current;
}

}  // namespace snapocr::annotate
