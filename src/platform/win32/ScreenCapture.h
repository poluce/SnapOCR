#pragma once

#include <QString>

#include "capture/CaptureTypes.h"

namespace snapocr::platform::win32 {

// GDI 逐显示器捕获：按物理像素抓取，不做任何缩放，不包含覆盖层（需在覆盖层创建前调用）。
class ScreenCapture
{
public:
    static bool captureAll(capture::CaptureResult* result, QString* errorMessage);
};

}  // namespace snapocr::platform::win32
