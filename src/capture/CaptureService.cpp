#include "capture/CaptureService.h"

#include "platform/win32/CursorCapture.h"
#include "platform/win32/ScreenCapture.h"

namespace snapocr::capture {

CaptureService::CaptureService(QObject* parent)
    : QObject(parent)
{
}

void CaptureService::captureAllScreens(bool includeCursor)
{
    capture::CaptureResult result;
    QString error;
    if (!platform::win32::ScreenCapture::captureAll(&result, &error)) {
        emit failed(error.isEmpty() ? QStringLiteral("屏幕捕获失败") : error);
        return;
    }
    if (includeCursor) {
        // 必须在覆盖层显示之前抓完并画好，否则会截到我们自己的窗口。
        platform::win32::drawCursorIntoCapture(&result);
    }
    emit captured(result);
}

}  // namespace snapocr::capture
