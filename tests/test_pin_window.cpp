#include <QtTest>

#include <QImage>
#include <QSignalSpy>
#include <QWheelEvent>

#include "ui/PinWindow.h"

using namespace snapocr::ui;

// 贴图窗口（F-ANN-19）：缩放/旋转/尺寸联动，缩放必须夹在 0.1x–4x。
class PinWindowTest final : public QObject
{
    Q_OBJECT

private slots:
    void initialSizeMatchesImage();
    void zoomScalesWindowAndClamps();
    void rotateSwapsDimensions();
    void imageIsKeptIntact();
    void requestOcrEmitsImage();
    void requestEditEmitsImage();

private:
    static QImage makeImage();
};

QImage PinWindowTest::makeImage()
{
    QImage image(160, 100, QImage::Format_ARGB32);
    image.fill(QColor(0x1F, 0x6F, 0xEB));
    return image;
}

void PinWindowTest::initialSizeMatchesImage()
{
    PinWindow pin(makeImage());
    QCOMPARE(pin.size(), QSize(160, 100));
    QCOMPARE(pin.zoom(), 1.0);
}

void PinWindowTest::zoomScalesWindowAndClamps()
{
    PinWindow pin(makeImage());
    pin.setZoom(2.0);
    QCOMPARE(pin.size(), QSize(320, 200));

    pin.setZoom(0.5);
    QCOMPARE(pin.size(), QSize(80, 50));

    // 上下限夹取
    pin.setZoom(100.0);
    QCOMPARE(pin.zoom(), 4.0);
    pin.setZoom(0.0001);
    QCOMPARE(pin.zoom(), 0.1);
}

void PinWindowTest::rotateSwapsDimensions()
{
    PinWindow pin(makeImage());
    QCOMPARE(pin.size(), QSize(160, 100));

    // 通过右键菜单以外的方式触发旋转：直接发 R 键
    QKeyEvent key(QEvent::KeyPress, Qt::Key_R, Qt::NoModifier);
    QApplication::sendEvent(&pin, &key);
    QCOMPARE(pin.size(), QSize(100, 160));

    QApplication::sendEvent(&pin, &key);
    QCOMPARE(pin.size(), QSize(160, 100));
}

void PinWindowTest::imageIsKeptIntact()
{
    const QImage source = makeImage();
    PinWindow pin(source);
    QCOMPARE(pin.image().size(), source.size());
    QCOMPARE(pin.image().pixelColor(10, 10), source.pixelColor(10, 10));
}

void PinWindowTest::requestOcrEmitsImage()
{
    const QImage source = makeImage();
    PinWindow pin(source);
    QSignalSpy spy(&pin, &PinWindow::ocrRequested);
    pin.requestOcr();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().first().value<QImage>().size(), source.size());
}

void PinWindowTest::requestEditEmitsImage()
{
    const QImage source = makeImage();
    PinWindow pin(source);
    QSignalSpy spy(&pin, &PinWindow::editRequested);
    pin.requestEdit();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().first().value<QImage>().size(), source.size());
}

QTEST_MAIN(PinWindowTest)

#include "test_pin_window.moc"
