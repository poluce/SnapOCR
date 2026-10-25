#include "platform/win32/CursorCapture.h"

#include <QDebug>
#include <QPainter>

#include <windows.h>

namespace snapocr::platform::win32 {
namespace {

// 用 GetDIBits 把位图读成 32 位 ARGB 的 QImage（top-down）。
QImage bitmapToImage(HBITMAP bitmap, int width, int height)
{
    if (bitmap == nullptr || width <= 0 || height <= 0) {
        return {};
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // 负值 = top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    HDC screenDc = ::GetDC(nullptr);
    if (screenDc == nullptr) {
        return {};
    }
    const int scanned =
        ::GetDIBits(screenDc, bitmap, 0, static_cast<UINT>(height), image.bits(), &info,
                    DIB_RGB_COLORS);
    ::ReleaseDC(nullptr, screenDc);

    if (scanned == 0) {
        return {};
    }
    return image;
}

}  // namespace

QImage iconToImage(void* hicon, QPoint* hotspot)
{
    if (hicon == nullptr) {
        return {};
    }
    const HICON icon = static_cast<HICON>(hicon);

    ICONINFO iconInfo{};
    if (::GetIconInfo(icon, &iconInfo) == FALSE) {
        return {};
    }

    BITMAP maskBitmap{};
    if (::GetObject(iconInfo.hbmMask, sizeof(BITMAP), &maskBitmap) == 0) {
        if (iconInfo.hbmColor != nullptr) {
            ::DeleteObject(iconInfo.hbmColor);
        }
        ::DeleteObject(iconInfo.hbmMask);
        return {};
    }

    const int width = maskBitmap.bmWidth;
    // 单色光标没有颜色位图，掩码高度是图标高度的两倍（AND 掩码 + XOR 掩码）。
    const int height = iconInfo.hbmColor != nullptr ? maskBitmap.bmHeight : maskBitmap.bmHeight / 2;

    const QImage mask = bitmapToImage(iconInfo.hbmMask, width, height);
    const QImage color =
        iconInfo.hbmColor != nullptr ? bitmapToImage(iconInfo.hbmColor, width, height) : QImage();

    if (hotspot != nullptr) {
        *hotspot = QPoint(static_cast<int>(iconInfo.xHotspot), static_cast<int>(iconInfo.yHotspot));
    }

    if (iconInfo.hbmColor != nullptr) {
        ::DeleteObject(iconInfo.hbmColor);
    }
    ::DeleteObject(iconInfo.hbmMask);

    if (mask.isNull()) {
        return {};
    }

    QImage result(width, height, QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    for (int y = 0; y < height; ++y) {
        const auto* maskLine = reinterpret_cast<const QRgb*>(mask.constScanLine(y));
        QRgb* outLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        const QRgb* colorLine =
            color.isNull() ? nullptr : reinterpret_cast<const QRgb*>(color.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            // 掩码为黑（0）表示该像素显示颜色，为白表示透明。
            const bool transparent = qGray(maskLine[x]) > 128;
            if (transparent) {
                continue;
            }
            if (colorLine != nullptr) {
                const QRgb c = colorLine[x];
                outLine[x] = qRgba(qRed(c), qGreen(c), qBlue(c), 255);
            } else {
                outLine[x] = qRgba(255, 255, 255, 255);  // 单色光标
            }
        }
    }
    return result;
}

void drawCursorIntoCapture(capture::CaptureResult* result)
{
    if (result == nullptr || result->screens.isEmpty()) {
        return;
    }

    CURSORINFO cursorInfo{};
    cursorInfo.cbSize = sizeof(CURSORINFO);
    if (::GetCursorInfo(&cursorInfo) == FALSE) {
        return;
    }
    if ((cursorInfo.flags & CURSOR_SHOWING) == 0 || cursorInfo.hCursor == nullptr) {
        return;  // 光标隐藏（例如正在输入或全屏播放）时不画
    }

    QPoint hotspot;
    const QImage cursor = iconToImage(cursorInfo.hCursor, &hotspot);
    if (cursor.isNull()) {
        return;
    }

    const QPoint cursorPhysical(static_cast<int>(cursorInfo.ptScreenPos.x),
                                static_cast<int>(cursorInfo.ptScreenPos.y));

    for (capture::ScreenFrame& frame : result->screens) {
        const QRect geometry = frame.monitor.physicalGeometry;
        if (!geometry.contains(cursorPhysical)) {
            continue;
        }
        const QPoint local = cursorPhysical - geometry.topLeft() - hotspot;
        // 光标可能跨出屏幕边缘，QPainter 会自动裁剪。
        QPainter painter(&frame.image);
        painter.drawImage(local, cursor);
        painter.end();
        return;
    }
}

}  // namespace snapocr::platform::win32
