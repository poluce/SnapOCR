#include <QtTest>

#include <QImage>
#include <QPoint>

#include <windows.h>

#include "platform/win32/CursorCapture.h"

// 光标捕获（F-CAP-12）：
// 把 HICON 转成带 alpha 的 QImage 是纯 Win32 逻辑，可以用系统图标稳定测试。
class CursorCaptureTest final : public QObject
{
    Q_OBJECT

private slots:
    void systemIconConvertsToImage();
    void nullIconReturnsEmpty();
    void visibleAreaHasOpaquePixels();
};

void CursorCaptureTest::systemIconConvertsToImage()
{
    // 系统默认应用图标（一定存在），用来验证 颜色位图 + 掩码 的合成逻辑。
    HICON icon = ::LoadIconW(nullptr, IDI_APPLICATION);
    QVERIFY(icon != nullptr);

    QPoint hotspot(-1, -1);
    const QImage image = snapocr::platform::win32::iconToImage(icon, &hotspot);

    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0);
    QVERIFY(image.height() > 0);
    QVERIFY(hotspot.x() >= 0);
    QVERIFY(hotspot.y() >= 0);
    QVERIFY(hotspot.x() < image.width());
    QVERIFY(hotspot.y() < image.height());
}

void CursorCaptureTest::nullIconReturnsEmpty()
{
    QPoint hotspot;
    const QImage image = snapocr::platform::win32::iconToImage(nullptr, &hotspot);
    QVERIFY(image.isNull());
}

void CursorCaptureTest::visibleAreaHasOpaquePixels()
{
    HICON icon = ::LoadIconW(nullptr, IDI_APPLICATION);
    QVERIFY(icon != nullptr);
    const QImage image = snapocr::platform::win32::iconToImage(icon, nullptr);
    QVERIFY(!image.isNull());

    int opaque = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 200) {
                ++opaque;
            }
        }
    }
    // 图标一定有可见像素，否则说明 alpha 合成把整张图都弄透明了。
    QVERIFY(opaque > 0);
}

QTEST_MAIN(CursorCaptureTest)

#include "test_cursor_capture.moc"
