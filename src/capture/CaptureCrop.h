#pragma once

#include <QImage>
#include <QRect>

#include "capture/CaptureTypes.h"

namespace snapocr::capture {

// 按物理像素矩形从多屏捕获结果裁剪/拼接出图像。
// 选区跨屏时逐屏 blit（显示器之间不重叠），输出尺寸恒等于 physicalRect 尺寸。
QImage cropSelection(const CaptureResult& result, const QRect& physicalRect);

}  // namespace snapocr::capture
