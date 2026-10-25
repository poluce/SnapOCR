#include <QtTest>

#include <QGuiApplication>
#include <QWidget>

#include "platform/win32/WindowDetector.h"

using namespace snapocr::platform::win32;

// 窗口识别（F-CAP-15）的纯几何规则：
// - 太小的窗口不吸附（避免点到边角就选中 1px 的隐形窗口）
// - 覆盖整个桌面的“窗口”就是桌面本身，不吸附
// - 超出虚拟桌面的部分要裁剪掉
class WindowDetectorTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsTooSmallRect();
    void rejectsFullDesktopWindow();
    void acceptsNormalWindow();
    void clampsToVirtualDesktop();
    void clampReturnsEmptyWhenOutside();
    void enumeratesRealWindowAtPoint();
};

void WindowDetectorTest::rejectsTooSmallRect()
{
    const QRect desktop(0, 0, 3840, 2160);
    QVERIFY(!isSnippingTargetRectValid(QRect(100, 100, 4, 4), desktop));
    QVERIFY(!isSnippingTargetRectValid(QRect(100, 100, 7, 200), desktop));
    QVERIFY(!isSnippingTargetRectValid(QRect(), desktop));
}

void WindowDetectorTest::rejectsFullDesktopWindow()
{
    const QRect desktop(0, 0, 3840, 2160);
    QVERIFY(!isSnippingTargetRectValid(desktop, desktop));
    // 覆盖甚至超出桌面也算桌面本身
    QVERIFY(!isSnippingTargetRectValid(QRect(-100, -100, 4200, 2400), desktop));
}

void WindowDetectorTest::acceptsNormalWindow()
{
    const QRect desktop(0, 0, 3840, 2160);
    QVERIFY(isSnippingTargetRectValid(QRect(200, 150, 800, 600), desktop));
    QVERIFY(isSnippingTargetRectValid(QRect(0, 0, 1920, 1080), desktop));  // 半屏窗口
}

void WindowDetectorTest::clampsToVirtualDesktop()
{
    const QRect desktop(0, 0, 3840, 2160);
    const QRect clamped = clampToVirtualDesktop(QRect(-50, -50, 400, 300), desktop);
    QCOMPARE(clamped, QRect(0, 0, 350, 250));

    const QRect right = clampToVirtualDesktop(QRect(3700, 2000, 400, 300), desktop);
    QCOMPARE(right, QRect(3700, 2000, 140, 160));
}

void WindowDetectorTest::clampReturnsEmptyWhenOutside()
{
    const QRect desktop(0, 0, 3840, 2160);
    QVERIFY(clampToVirtualDesktop(QRect(5000, 5000, 100, 100), desktop).isEmpty());
    QVERIFY(clampToVirtualDesktop(QRect(), desktop).isEmpty());
}

void WindowDetectorTest::enumeratesRealWindowAtPoint()
{
    if (QGuiApplication::platformName() != QLatin1String("windows")) {
        QSKIP("仅在真实 windows 平台验证窗口枚举");
    }
    QWidget probe;
    probe.setWindowTitle(QStringLiteral("SnapOCR WindowDetector Probe"));
    probe.setGeometry(200, 200, 320, 240);
    probe.show();
    QVERIFY(QTest::qWaitForWindowExposed(&probe));
    QTest::qWait(80);

    // Qt 坐标是逻辑像素，窗口检测用物理像素：按屏幕 DPR 换算
    const qreal dpr = probe.screen() != nullptr ? probe.screen()->devicePixelRatio() : 1.0;
    const QPoint logicalCenter = probe.mapToGlobal(probe.rect().center());
    const QPoint center(qRound(logicalCenter.x() * dpr), qRound(logicalCenter.y() * dpr));
    const QVector<WindowTarget> found = enumerateWindowsAt(center, 8, true);
    QStringList debug;
    debug << QStringLiteral("count=%1 platform=%2").arg(found.size()).arg(QGuiApplication::platformName());
    for (const WindowTarget& target : found) {
        debug << QStringLiteral("[%1] '%2' %3x%4 at %5,%6")
                     .arg(target.className)
                     .arg(target.title)
                     .arg(target.physicalGeometry.width())
                     .arg(target.physicalGeometry.height())
                     .arg(target.physicalGeometry.x())
                     .arg(target.physicalGeometry.y());
    }
    debug << QStringLiteral("probe hwnd=%1 geom=%2,%3 %4x%5 visible=%6")
                 .arg(QString::number(static_cast<qulonglong>(probe.winId())))
                 .arg(probe.geometry().x())
                 .arg(probe.geometry().y())
                 .arg(probe.geometry().width())
                 .arg(probe.geometry().height())
                 .arg(probe.isVisible() ? 1 : 0);
    QVERIFY2(!found.isEmpty(), qPrintable(debug.join(QStringLiteral(" | "))));

    bool matched = false;
    for (const WindowTarget& target : found) {
        if (target.handle == static_cast<quintptr>(probe.winId())) {
            matched = true;
            QVERIFY(target.physicalGeometry.contains(center));
            QCOMPARE(target.title, QStringLiteral("SnapOCR WindowDetector Probe"));
            break;
        }
    }
    QVERIFY2(matched, "枚举结果里没有探针窗口");
}

QTEST_MAIN(WindowDetectorTest)

#include "test_window_detector.moc"
