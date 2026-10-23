#include <QtTest>

#include <QImage>

#include "capture/CaptureCrop.h"

using namespace snapocr::capture;

namespace {

CaptureResult makeTwoScreenResult()
{
    CaptureResult result;

    ScreenFrame left;
    left.monitor.id = QStringLiteral("left");
    left.monitor.physicalGeometry = QRect(-200, 0, 200, 100);
    left.image = QImage(200, 100, QImage::Format_ARGB32);
    left.image.fill(QColor(Qt::red));

    ScreenFrame main;
    main.monitor.id = QStringLiteral("main");
    main.monitor.physicalGeometry = QRect(0, 0, 400, 100);
    main.image = QImage(400, 100, QImage::Format_ARGB32);
    main.image.fill(QColor(Qt::green));

    result.screens = {left, main};
    result.virtualGeometry = QRect(-200, 0, 600, 100);
    return result;
}

}  // namespace

class CaptureCropTest final : public QObject
{
    Q_OBJECT

private slots:
    void cropsSingleScreen();
    void cropsAcrossScreens();
    void returnsEmptyForEmptyRect();
    void keepsOutputSizeEqualToSelection();
};

void CaptureCropTest::cropsSingleScreen()
{
    const CaptureResult result = makeTwoScreenResult();
    const QImage cropped = cropSelection(result, QRect(10, 10, 50, 30));
    QCOMPARE(cropped.size(), QSize(50, 30));
    QCOMPARE(cropped.pixelColor(0, 0), QColor(Qt::green));
}

void CaptureCropTest::cropsAcrossScreens()
{
    const CaptureResult result = makeTwoScreenResult();
    // 从左侧屏 (-50..) 跨越到主屏 (..50)
    const QImage cropped = cropSelection(result, QRect(-50, 0, 100, 50));
    QCOMPARE(cropped.size(), QSize(100, 50));
    QCOMPARE(cropped.pixelColor(0, 0), QColor(Qt::red));      // 左侧屏部分
    QCOMPARE(cropped.pixelColor(99, 0), QColor(Qt::green));   // 主屏部分
}

void CaptureCropTest::returnsEmptyForEmptyRect()
{
    const CaptureResult result = makeTwoScreenResult();
    QVERIFY(cropSelection(result, QRect()).isNull());
}

void CaptureCropTest::keepsOutputSizeEqualToSelection()
{
    const CaptureResult result = makeTwoScreenResult();
    // 选区超出可用范围时仍按请求尺寸输出（空白处为黑色）。
    const QImage cropped = cropSelection(result, QRect(0, 0, 800, 200));
    QCOMPARE(cropped.size(), QSize(800, 200));
}

QTEST_GUILESS_MAIN(CaptureCropTest)

#include "test_capture_crop.moc"
