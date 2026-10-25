#include <QtTest>

#include <QImage>
#include <QSignalSpy>

#include "capture/ScrollingCaptureController.h"

using namespace snapocr::capture;

// 滚动长截图的抓帧状态机（F-CAP-16）：
// 用注入的 FrameProvider 提供“已知长图的切片”，验证抓帧→自动结束→拼接全流程。
class ScrollingCaptureTest final : public QObject
{
    Q_OBJECT

private slots:
    void failsWhenProviderFails();
    void finishesWithStitchedImage();
    void autoFinishesWhenPageStops();
    void cancelDiscardsFrames();

private:
    static QImage makeLongImage(int width, int height);
};

QImage ScrollingCaptureTest::makeLongImage(int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32);
    quint32 state = 0x89ABCDEFu;
    for (int y = 0; y < height; ++y) {
        state = state * 1664525u + 1013904223u;
        const QRgb row = qRgb((state >> 8) & 0xFF, (state >> 16) & 0xFF, (state >> 24) & 0xFF);
        for (int x = 0; x < width; ++x) {
            image.setPixel(x, y, row);
        }
    }
    return image;
}

void ScrollingCaptureTest::failsWhenProviderFails()
{
    ScrollingCaptureController controller;
    controller.setFrameProvider([](QImage*, QString* error) {
        if (error != nullptr) {
            *error = QStringLiteral("屏幕被锁定");
        }
        return false;
    });
    QSignalSpy failedSpy(&controller, &ScrollingCaptureController::failed);

    controller.start(QRect(0, 0, 100, 200));
    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(failedSpy.takeFirst().first().toString().contains(QStringLiteral("锁定")));
    QVERIFY(!controller.isActive());
}

void ScrollingCaptureTest::finishesWithStitchedImage()
{
    const QImage longImage = makeLongImage(80, 1500);
    int call = 0;
    ScrollingCaptureController controller;
    controller.setIntervalMs(1000);  // 测试里手动触发，避免定时器干扰
    controller.setFrameProvider([&](QImage* image, QString*) {
        const int top = qMin(call * 200, longImage.height() - 500);
        ++call;
        *image = longImage.copy(0, top, 80, 500);
        return true;
    });

    QSignalSpy finishedSpy(&controller, &ScrollingCaptureController::finished);
    controller.start(QRect(0, 0, 80, 500));
    QCOMPARE(controller.frameCount(), 1);  // start 立即抓第一帧

    // 再手动抓几帧（模拟定时器触发）：通过 finish 前的 provider 调用
    for (int i = 0; i < 4; ++i) {
        QMetaObject::invokeMethod(&controller, "captureFrame");
    }
    QVERIFY(controller.frameCount() >= 2);

    controller.finish();
    QCOMPARE(finishedSpy.count(), 1);
    const QImage stitched = finishedSpy.takeFirst().first().value<QImage>();
    QCOMPARE(stitched.width(), 80);
    QVERIFY(stitched.height() > 500);  // 确实拼接变长了
}

void ScrollingCaptureTest::autoFinishesWhenPageStops()
{
    const QImage frame = makeLongImage(60, 400);
    ScrollingCaptureController controller;
    controller.setIntervalMs(1000);
    controller.setFrameProvider([&](QImage* image, QString*) {
        *image = frame;  // 一直返回同一帧：页面没滚动
        return true;
    });

    QSignalSpy finishedSpy(&controller, &ScrollingCaptureController::finished);
    controller.start(QRect(0, 0, 60, 400));
    // 连续相同帧达到阈值后自动结束
    for (int i = 0; i < 4; ++i) {
        QMetaObject::invokeMethod(&controller, "captureFrame");
    }
    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(!controller.isActive());
    const QImage stitched = finishedSpy.takeFirst().first().value<QImage>();
    QCOMPARE(stitched.size(), frame.size());
}

void ScrollingCaptureTest::cancelDiscardsFrames()
{
    const QImage frame = makeLongImage(40, 200);
    ScrollingCaptureController controller;
    controller.setFrameProvider([&](QImage* image, QString*) {
        *image = frame;
        return true;
    });
    QSignalSpy finishedSpy(&controller, &ScrollingCaptureController::finished);
    QSignalSpy failedSpy(&controller, &ScrollingCaptureController::failed);

    controller.start(QRect(0, 0, 40, 200));
    QVERIFY(controller.isActive());
    controller.cancel();
    QVERIFY(!controller.isActive());
    controller.finish();  // 取消后 finish 不应再有输出
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
}

QTEST_MAIN(ScrollingCaptureTest)

#include "test_scrolling_capture.moc"
