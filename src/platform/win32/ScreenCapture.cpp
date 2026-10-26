#include "platform/win32/ScreenCapture.h"

#include <utility>

#include <QDebug>
#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QVector>

#include <windows.h>

#include "capture/MonitorLayout.h"

namespace snapocr::platform::win32 {
namespace {

QString win32ErrorMessage(DWORD code)
{
    wchar_t* buffer = nullptr;
    const DWORD length = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                              | FORMAT_MESSAGE_IGNORE_INSERTS,
                                          nullptr,
                                          code,
                                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                          reinterpret_cast<LPWSTR>(&buffer),
                                          0,
                                          nullptr);
    QString message;
    if (length > 0 && buffer != nullptr) {
        message = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
    }
    if (buffer != nullptr) {
        ::LocalFree(buffer);
    }
    if (message.isEmpty()) {
        message = QStringLiteral("Win32 错误码 %1").arg(code);
    }
    return message;
}

}  // namespace

bool ScreenCapture::captureAll(capture::CaptureResult* result, QString* errorMessage)
{
    if (result == nullptr) {
        return false;
    }
    result->screens.clear();

    QElapsedTimer timer;
    timer.start();

    const QVector<capture::MonitorInfo> monitors = capture::MonitorLayout::enumerate();
    if (monitors.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("未检测到显示器");
        }
        return false;
    }

    HDC screenDc = ::GetDC(nullptr);
    if (screenDc == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("获取屏幕 DC 失败：%1").arg(win32ErrorMessage(::GetLastError()));
        }
        return false;
    }

    capture::CaptureResult captured;
    QString lastError;

    for (const capture::MonitorInfo& monitor : monitors) {
        const QRect geometry = monitor.physicalGeometry;
        if (geometry.width() <= 0 || geometry.height() <= 0) {
            continue;
        }

        HDC memoryDc = ::CreateCompatibleDC(screenDc);
        if (memoryDc == nullptr) {
            lastError = QStringLiteral("创建兼容 DC 失败：%1").arg(win32ErrorMessage(::GetLastError()));
            continue;
        }

        // 关键：副屏（尤其位于主屏左侧形成负坐标、或混合 DPI/旋转）不能用屏幕 DC
        // 的虚拟坐标直接 BitBlt——它会静默返回成功但内容错位/为主屏内容。
        // 正确做法：为每块显示器创建独立设备 DC（\\.\DISPLAYn），源坐标从 (0,0) 取。
        const std::wstring deviceName = monitor.id.toStdWString();
        HDC deviceDc = ::CreateDCW(deviceName.c_str(), nullptr, nullptr, nullptr);

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = geometry.width();
        bitmapInfo.bmiHeader.biHeight = -geometry.height();  // 负值 = top-down
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP bitmap = ::CreateDIBSection(memoryDc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bitmap == nullptr || bits == nullptr) {
            lastError = QStringLiteral("创建 DIB 失败：%1").arg(win32ErrorMessage(::GetLastError()));
            if (bitmap != nullptr) {
                ::DeleteObject(bitmap);
            }
            ::DeleteDC(memoryDc);
            continue;
        }

        const HGDIOBJ previous = ::SelectObject(memoryDc, bitmap);
        BOOL copied = FALSE;
        if (deviceDc != nullptr) {
            copied = ::BitBlt(memoryDc, 0, 0, geometry.width(), geometry.height(), deviceDc,
                              0, 0, SRCCOPY | CAPTUREBLT);
        }
        if (copied == FALSE) {
            // 回退：设备 DC 不可用时用屏幕 DC + 虚拟坐标（主屏、右侧副屏通常可用）
            copied = ::BitBlt(memoryDc,
                              0,
                              0,
                              geometry.width(),
                              geometry.height(),
                              screenDc,
                              geometry.x(),
                              geometry.y(),
                              SRCCOPY | CAPTUREBLT);
        }
        ::SelectObject(memoryDc, previous);
        if (deviceDc != nullptr) {
            ::DeleteDC(deviceDc);
            deviceDc = nullptr;
        }

        if (copied == FALSE) {
            lastError = QStringLiteral("BitBlt 失败：%1").arg(win32ErrorMessage(::GetLastError()));
            qWarning("capture monitor %s failed: BitBlt (phys=(%d,%d %dx%d))",
                     qUtf8Printable(monitor.id), geometry.x(), geometry.y(), geometry.width(),
                     geometry.height());
            ::DeleteObject(bitmap);
            ::DeleteDC(memoryDc);
            continue;
        }

        const QImage view(static_cast<const uchar*>(bits),
                          geometry.width(),
                          geometry.height(),
                          geometry.width() * 4,
                          QImage::Format_RGB32);
        capture::ScreenFrame frame;
        frame.monitor = monitor;
        frame.image = view.copy();  // 深拷贝，DIB 对象随后释放
        qInfo("capture monitor %s phys=(%d,%d %dx%d) logical=(%d,%d %dx%d) dpr=%.2f image=%dx%d",
              qUtf8Printable(monitor.id),
              monitor.physicalGeometry.x(), monitor.physicalGeometry.y(),
              monitor.physicalGeometry.width(), monitor.physicalGeometry.height(),
              monitor.logicalGeometry.x(), monitor.logicalGeometry.y(),
              monitor.logicalGeometry.width(), monitor.logicalGeometry.height(),
              monitor.devicePixelRatio, frame.image.width(), frame.image.height());

        ::DeleteObject(bitmap);
        ::DeleteDC(memoryDc);

        if (!frame.image.isNull()) {
            captured.screens.append(frame);
        }
    }

    ::ReleaseDC(nullptr, screenDc);

    if (captured.screens.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = lastError.isEmpty() ? QStringLiteral("屏幕捕获失败") : lastError;
        }
        return false;
    }

    captured.virtualGeometry = capture::MonitorLayout::virtualPhysicalGeometry(monitors);
    captured.capturedAtMs = timer.elapsed();
    *result = std::move(captured);
    return true;
}


bool ScreenCapture::captureRegion(const QRect& physicalRect, QImage* image,
                                  QString* errorMessage)
{
    if (image == nullptr) {
        return false;
    }
    if (physicalRect.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("截图区域为空");
        }
        return false;
    }

    // 裁剪到虚拟桌面，保证 BitBlt 源坐标合法
    const QRect virtualRect(::GetSystemMetrics(SM_XVIRTUALSCREEN), ::GetSystemMetrics(SM_YVIRTUALSCREEN),
                            ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
                            ::GetSystemMetrics(SM_CYVIRTUALSCREEN));
    const QRect region = virtualRect.isEmpty() ? physicalRect : physicalRect.intersected(virtualRect);
    if (region.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("截图区域不在屏幕内");
        }
        return false;
    }

    HDC screenDc = ::GetDC(nullptr);
    if (screenDc == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("获取屏幕 DC 失败");
        }
        return false;
    }
    HDC memoryDc = ::CreateCompatibleDC(screenDc);
    if (memoryDc == nullptr) {
        ::ReleaseDC(nullptr, screenDc);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("创建兼容 DC 失败");
        }
        return false;
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = region.width();
    bitmapInfo.bmiHeader.biHeight = -region.height();
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = ::CreateDIBSection(memoryDc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap == nullptr || bits == nullptr) {
        if (bitmap != nullptr) {
            ::DeleteObject(bitmap);
        }
        ::DeleteDC(memoryDc);
        ::ReleaseDC(nullptr, screenDc);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("创建 DIB 失败");
        }
        return false;
    }

    const HGDIOBJ previous = ::SelectObject(memoryDc, bitmap);
    BOOL copied = FALSE;
    // 找到区域所在显示器，用其设备 DC + 局部坐标（避免副屏负坐标问题）
    const QVector<capture::MonitorInfo> monitors = capture::MonitorLayout::enumerate();
    for (const capture::MonitorInfo& monitor : monitors) {
        const QRect physical = monitor.physicalGeometry;
        if (!physical.contains(region)) {
            continue;
        }
        const std::wstring deviceName = monitor.id.toStdWString();
        HDC deviceDc = ::CreateDCW(deviceName.c_str(), nullptr, nullptr, nullptr);
        if (deviceDc != nullptr) {
            copied = ::BitBlt(memoryDc, 0, 0, region.width(), region.height(), deviceDc,
                              region.x() - physical.x(), region.y() - physical.y(),
                              SRCCOPY | CAPTUREBLT);
            ::DeleteDC(deviceDc);
        }
        break;
    }
    if (copied == FALSE) {
        copied = ::BitBlt(memoryDc, 0, 0, region.width(), region.height(), screenDc,
                          region.x(), region.y(), SRCCOPY | CAPTUREBLT);
    }
    ::SelectObject(memoryDc, previous);

    bool ok = false;
    if (copied != FALSE) {
        const QImage view(static_cast<const uchar*>(bits), region.width(), region.height(),
                          region.width() * 4, QImage::Format_RGB32);
        *image = view.copy();
        ok = !image->isNull();
    } else if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("BitBlt 失败");
    }

    ::DeleteObject(bitmap);
    ::DeleteDC(memoryDc);
    ::ReleaseDC(nullptr, screenDc);
    return ok;
}

}  // namespace snapocr::platform::win32
