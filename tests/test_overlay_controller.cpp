#include <QtTest>

#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QSignalSpy>

#include "overlay/OverlayController.h"
#include "overlay/OverlayWindow.h"

using namespace snapocr;

// 覆盖层主链路集成测试：用合成的捕获结果驱动
// 「捕获 → 逐屏覆盖层 → 拖拽选区 → 确认/取消」整条链路，
// 这样这条只能靠真机手测的主路径也能在 offscreen 下自动回归。
class OverlayControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void beginCaptureShowsOverlay();
    void dragSelectionThenConfirmEmitsCrop();
    void cancelEmitsFinishedAndDeactivates();
    void confirmWithoutSelectionDoesNothing();

private:
    QRect screenGeometry() const;
    QImage makeScreenImage() const;
    void dragSelection(overlay::OverlayWindow* window, const QPoint& from, const QPoint& to);

    overlay::OverlayController* m_controller = nullptr;
};

namespace {

overlay::OverlayWindow* firstOverlay(overlay::OverlayController* controller)
{
    const auto overlays = controller->overlayWindows();
    return overlays.isEmpty() ? nullptr : overlays.first();
}

}  // namespace

QRect OverlayControllerTest::screenGeometry() const
{
    QScreen* screen = QGuiApplication::primaryScreen();
    return screen != nullptr ? screen->geometry() : QRect(0, 0, 800, 600);
}

QImage OverlayControllerTest::makeScreenImage() const
{
    const QRect geometry = screenGeometry();
    QImage image(geometry.size(), QImage::Format_ARGB32);
    // 每个像素颜色与坐标相关，便于验证裁剪区域是否对得上
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y, QColor(x % 256, y % 256, (x + y) % 256));
        }
    }
    return image;
}

void OverlayControllerTest::init()
{
    m_controller = new overlay::OverlayController;
    m_controller->setDetectWindows(false);
    const QRect geometry = screenGeometry();
    const QImage image = makeScreenImage();

    m_controller->setCaptureProvider(
        [geometry, image](bool, capture::CaptureResult* result, QString*) {
            capture::MonitorInfo monitor;
            monitor.id = QStringLiteral("test-monitor");
            monitor.displayName = QStringLiteral("Test");
            monitor.primary = true;
            monitor.physicalGeometry = geometry;
            monitor.logicalGeometry = geometry;
            monitor.devicePixelRatio = 1.0;
            monitor.rotationDegrees = 0;

            capture::ScreenFrame frame;
            frame.monitor = monitor;
            frame.image = image;

            result->screens.append(frame);
            result->virtualGeometry = geometry;
            result->capturedAtMs = 0;
            return true;
        });
}

void OverlayControllerTest::cleanup()
{
    if (m_controller != nullptr) {
        m_controller->cancel();
        delete m_controller;
        m_controller = nullptr;
    }
}

void OverlayControllerTest::dragSelection(overlay::OverlayWindow* window, const QPoint& from,
                                          const QPoint& to)
{
    QVERIFY(window != nullptr);
    const auto localFrom = from - window->geometry().topLeft();
    const auto localTo = to - window->geometry().topLeft();

    QMouseEvent press(QEvent::MouseButtonPress, localFrom, window->mapToGlobal(localFrom),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window, &press);

    QMouseEvent move(QEvent::MouseMove, localTo, window->mapToGlobal(localTo), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window, &move);

    QMouseEvent release(QEvent::MouseButtonRelease, localTo, window->mapToGlobal(localTo),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(window, &release);
}

void OverlayControllerTest::beginCaptureShowsOverlay()
{
    QSignalSpy startedSpy(m_controller, &overlay::OverlayController::started);
    m_controller->beginCapture();
    QVERIFY(m_controller->isActive());
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(firstOverlay(m_controller) != nullptr);
}

void OverlayControllerTest::dragSelectionThenConfirmEmitsCrop()
{
    QSignalSpy copySpy(m_controller, &overlay::OverlayController::copyRequested);
    m_controller->beginCapture();
    QVERIFY(m_controller->isActive());

    overlay::OverlayWindow* window = firstOverlay(m_controller);
    QVERIFY(window != nullptr);

    const QPoint from = screenGeometry().topLeft() + QPoint(40, 30);
    const QPoint to = screenGeometry().topLeft() + QPoint(240, 180);
    dragSelection(window, from, to);

    m_controller->requestConfirm();
    QCOMPARE(copySpy.count(), 1);
    const QImage copied = copySpy.takeFirst().first().value<QImage>();

    // 选区按物理像素「含两端点」语义：40..240 覆盖 201 列，30..180 覆盖 151 行
    const QSize expectedSize(to.x() - from.x() + 1, to.y() - from.y() + 1);
    QCOMPARE(copied.size(), expectedSize);

    // 像素与合成底图对应位置一致（证明裁剪坐标正确）
    const QImage expected = makeScreenImage().copy(QRect(from, expectedSize));
    QCOMPARE(copied.pixelColor(0, 0), expected.pixelColor(0, 0));
    QCOMPARE(copied.pixelColor(100, 75), expected.pixelColor(100, 75));
    QVERIFY(!m_controller->isActive());
}

void OverlayControllerTest::cancelEmitsFinishedAndDeactivates()
{
    QSignalSpy finishedSpy(m_controller, &overlay::OverlayController::finished);
    m_controller->beginCapture();
    QVERIFY(m_controller->isActive());

    overlay::OverlayWindow* window = firstOverlay(m_controller);
    QVERIFY(window != nullptr);
    dragSelection(window, screenGeometry().topLeft() + QPoint(10, 10),
                  screenGeometry().topLeft() + QPoint(100, 100));

    m_controller->requestCancel();
    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(!m_controller->isActive());
    QVERIFY(firstOverlay(m_controller) == nullptr);
}

void OverlayControllerTest::confirmWithoutSelectionDoesNothing()
{
    QSignalSpy copySpy(m_controller, &overlay::OverlayController::copyRequested);
    m_controller->beginCapture();
    QVERIFY(m_controller->isActive());

    // 没有可用选区时确认：不应复制，也不应崩溃（实现上等同于取消）
    m_controller->requestConfirm();
    QCOMPARE(copySpy.count(), 0);
    QVERIFY(!m_controller->isActive());
}

QTEST_MAIN(OverlayControllerTest)

#include "test_overlay_controller.moc"
