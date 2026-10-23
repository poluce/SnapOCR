#include "capture/CaptureService.h"

#include "platform/win32/ScreenCapture.h"

namespace snapocr::capture {

CaptureService::CaptureService(QObject* parent)
    : QObject(parent)
{
}

void CaptureService::captureAllScreens()
{
    capture::CaptureResult result;
    QString error;
    if (!platform::win32::ScreenCapture::captureAll(&result, &error)) {
        emit failed(error.isEmpty() ? QStringLiteral("屏幕捕获失败") : error);
        return;
    }
    emit captured(result);
}

}  // namespace snapocr::capture
