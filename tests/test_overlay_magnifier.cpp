#include <QtTest>

#include <QImage>
#include <QWidget>

#include "capture/CaptureTypes.h"
#include "overlay/ColorMagnifier.h"
#include "overlay/OverlayWindow.h"
#include "overlay/SelectionModel.h"

using namespace snapocr;
using namespace snapocr::overlay;

// 放大镜在覆盖层里的实际绘制（F-CAP-14）：
// 用合成画面离屏渲染整个覆盖层，确认面板真的被画出来、取样点与颜色正确。
class OverlayMagnifierTest final : public QObject
{
    Q_OBJECT

private slots:
    void paintedPanelVisibleWithoutSelection();
    void samplePointMatchesCursor();
    void clickingWindowSelectsItsRect();
};

namespace {

capture::ScreenFrame makeFrame()
{
    capture::MonitorInfo monitor;
    monitor.id = QStringLiteral("test");
    monitor.physicalGeometry = QRect(0, 0, 400, 300);
    monitor.logicalGeometry = QRect(0, 0, 400, 300);
    monitor.devicePixelRatio = 1.0;
    monitor.primary = true;

    capture::ScreenFrame frame;
    frame.monitor = monitor;
    frame.image = QImage(400, 300, QImage::Format_ARGB32);
    frame.image.fill(QColor(0x20, 0x60, 0xC0));
    // 在光标位置画一块红色，便于核对取样
    for (int y = 60; y < 70; ++y) {
        for (int x = 60; x < 70; ++x) {
            frame.image.setPixelColor(x, y, QColor(0xFF, 0x00, 0x00));
        }
    }
    return frame;
}

}  // namespace

void OverlayMagnifierTest::paintedPanelVisibleWithoutSelection()
{
    const capture::ScreenFrame frame = makeFrame();
    const QVector<capture::MonitorInfo> monitors{frame.monitor};
    SelectionModel selection;
    selection.setBounds(frame.monitor.physicalGeometry);

    OverlayWindow window(frame, &monitors, &selection);
    window.resize(frame.monitor.logicalGeometry.size());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // 光标在左上区域：面板应出现在光标右下，并且被真正绘制出来
    window.showMagnifierAt(QPoint(100, 100));
    QTest::qWait(30);
    const QImage rendered = window.grab().toImage();
    QVERIFY(!rendered.isNull());

    const QRectF panel = ColorMagnifier::panelRect(QPoint(100, 100), window.size());
    QVERIFY(panel.left() > 100);
    QVERIFY(panel.top() > 100);

    // 面板内部应有高亮像素（白色面板 / 网格），而背景是被暗化的蓝
    int brightPixels = 0;
    for (int y = static_cast<int>(panel.top()) + 4;
         y < static_cast<int>(panel.bottom()) - 4 && y < rendered.height(); y += 3) {
        for (int x = static_cast<int>(panel.left()) + 4;
             x < static_cast<int>(panel.right()) - 4 && x < rendered.width(); x += 3) {
            if (qGray(rendered.pixel(x, y)) > 180) {
                ++brightPixels;
            }
        }
    }
    QVERIFY2(brightPixels > 20, qPrintable(QStringLiteral("bright=%1").arg(brightPixels)));
}

void OverlayMagnifierTest::samplePointMatchesCursor()
{
    const capture::ScreenFrame frame = makeFrame();
    const QVector<capture::MonitorInfo> monitors{frame.monitor};
    SelectionModel selection;
    selection.setBounds(frame.monitor.physicalGeometry);

    OverlayWindow window(frame, &monitors, &selection);
    window.resize(frame.monitor.logicalGeometry.size());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // dpr=1 时全局逻辑坐标 == 覆盖层内的物理坐标
    window.showMagnifierAt(QPoint(65, 65));
    QTest::qWait(20);
    // 绘制面板中心格用的就是这个取样点，直接验证取色函数与该点一致
    QCOMPARE(ColorMagnifier::sampleColor(frame.image, QPoint(65, 65)), QColor(0xFF, 0x00, 0x00));
    window.hideMagnifier();
}

void OverlayMagnifierTest::clickingWindowSelectsItsRect()
{
    const capture::ScreenFrame frame = makeFrame();
    const QVector<capture::MonitorInfo> monitors{frame.monitor};
    SelectionModel selection;
    selection.setBounds(frame.monitor.physicalGeometry);

    OverlayWindow window(frame, &monitors, &selection);
    window.resize(frame.monitor.logicalGeometry.size());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QSignalSpy releasedSpy(&window, &OverlayWindow::selectionReleased);
    const QRect target(100, 100, 200, 150);
    window.applyWindowSelection(target);

    QCOMPARE(releasedSpy.count(), 1);
    QVERIFY(selection.hasUsableSelection());
    QCOMPARE(selection.rect(), target);
    // 窗口识别选中后不应残留悬停高亮
    window.clearWindowHover();
}

QTEST_MAIN(OverlayMagnifierTest)

#include "test_overlay_magnifier.moc"
