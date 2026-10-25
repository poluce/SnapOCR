#pragma once

#include <QImage>
#include <QPoint>

#include "capture/CaptureTypes.h"

namespace snapocr::platform::win32 {

// 把系统光标图标转成带 alpha 的 QImage。
// 说明：不用 DrawIconEx 画到 DIB —— 那样拿不到正确的 alpha（透明区域会变黑）。
// 这里按经典做法：颜色位图 + 掩码位图 手工合成 alpha。
// 返回空图表示光标不可用（隐藏 / 取图失败）。hotspot 为图标内的热点（像素）。
QImage iconToImage(void* hicon, QPoint* hotspot);

// 把当前鼠标光标画进捕获结果中光标所在的那块屏幕图像（物理像素坐标）。
// 光标不可见或没有捕获结果时静默返回。
void drawCursorIntoCapture(capture::CaptureResult* result);

}  // namespace snapocr::platform::win32
