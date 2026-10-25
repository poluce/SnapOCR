#include <QtTest>

#include <QImage>
#include <QPainter>

#include "overlay/ColorMagnifier.h"

using namespace snapocr::overlay;

// 放大镜取色（F-CAP-14）：
// - 取样必须精确命中物理像素，越界夹取而不是崩溃；
// - HEX/RGB 文本格式固定，复制到剪贴板的内容可预期；
// - 面板绘制位置会翻转/夹取，且渲染结果不是空白。
class ColorMagnifierTest final : public QObject
{
    Q_OBJECT

private slots:
    void sampleColorHitsExactPixel();
    void sampleColorClampsOutOfRange();
    void nullImageReturnsBlack();
    void hexAndRgbTextFormatting();
    void panelRectFlipsNearRightBottom();
    void paintProducesVisiblePanel();

private:
    static QImage makeFrame();
};

QImage ColorMagnifierTest::makeFrame()
{
    QImage image(20, 12, QImage::Format_ARGB32);
    image.fill(Qt::black);
    image.setPixelColor(0, 0, QColor(0xFF, 0x00, 0x00));
    image.setPixelColor(5, 4, QColor(0x1F, 0x6F, 0xEB));
    image.setPixelColor(19, 11, QColor(0x12, 0x34, 0x56));
    return image;
}

void ColorMagnifierTest::sampleColorHitsExactPixel()
{
    const QImage frame = makeFrame();
    QCOMPARE(ColorMagnifier::sampleColor(frame, QPoint(5, 4)), QColor(0x1F, 0x6F, 0xEB));
    QCOMPARE(ColorMagnifier::sampleColor(frame, QPoint(0, 0)), QColor(0xFF, 0x00, 0x00));
    QCOMPARE(ColorMagnifier::sampleColor(frame, QPoint(19, 11)), QColor(0x12, 0x34, 0x56));
}

void ColorMagnifierTest::sampleColorClampsOutOfRange()
{
    const QImage frame = makeFrame();
    QCOMPARE(ColorMagnifier::sampleColor(frame, QPoint(-5, -5)), QColor(0xFF, 0x00, 0x00));
    QCOMPARE(ColorMagnifier::sampleColor(frame, QPoint(999, 999)), QColor(0x12, 0x34, 0x56));
}

void ColorMagnifierTest::nullImageReturnsBlack()
{
    QCOMPARE(ColorMagnifier::sampleColor(QImage(), QPoint(3, 3)), QColor(Qt::black));
}

void ColorMagnifierTest::hexAndRgbTextFormatting()
{
    QCOMPARE(ColorMagnifier::hexText(QColor(0x1F, 0x6F, 0xEB)), QStringLiteral("#1F6FEB"));
    QCOMPARE(ColorMagnifier::rgbText(QColor(0x1F, 0x6F, 0xEB)),
             QStringLiteral("RGB 31, 111, 235"));
}

void ColorMagnifierTest::panelRectFlipsNearRightBottom()
{
    const QSize parent(1920, 1080);
    const QSize size = ColorMagnifier::panelSize();
    QVERIFY(size.width() > 60);
    QVERIFY(size.height() > 60);

    // 左上角：面板放在光标右下
    const QRectF topLeft = ColorMagnifier::panelRect(QPoint(100, 100), parent);
    QVERIFY(topLeft.left() >= 100);
    QVERIFY(topLeft.top() >= 100);

    // 右下角：面板翻到光标左上，并且不超出窗口
    const QRectF bottomRight = ColorMagnifier::panelRect(QPoint(parent.width() - 5, parent.height() - 5), parent);
    QVERIFY(bottomRight.right() <= parent.width());
    QVERIFY(bottomRight.bottom() <= parent.height());
    QVERIFY(bottomRight.left() < parent.width() - 5);
}

void ColorMagnifierTest::paintProducesVisiblePanel()
{
    const QImage frame = makeFrame();
    QImage canvas(300, 300, QImage::Format_ARGB32);
    canvas.fill(QColor(0x20, 0x20, 0x20));

    QPainter painter(&canvas);
    const QRectF panel(50, 50, ColorMagnifier::panelSize().width(),
                       ColorMagnifier::panelSize().height());
    ColorMagnifier::paint(painter, panel, frame, QPoint(5, 4));
    painter.end();

    // 面板区域必须有非背景像素（白色面板 + 取样色块）
    int bright = 0;
    for (int y = 52; y < 50 + panel.height() - 2 && y < canvas.height(); y += 3) {
        for (int x = 52; x < 50 + panel.width() - 2 && x < canvas.width(); x += 3) {
            if (qGray(canvas.pixel(x, y)) > 200) {
                ++bright;
            }
        }
    }
    QVERIFY(bright > 0);
}

QTEST_MAIN(ColorMagnifierTest)

#include "test_color_magnifier.moc"
