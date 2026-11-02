#include <QtTest>

#include <QVector>

#include "capture/MonitorLayout.h"

using namespace snapocr::capture;

// Win32 显示器与 Qt 屏幕的配对逻辑（可脱离真实多屏环境回归）。
class MonitorLayoutTest final : public QObject
{
    Q_OBJECT

private slots:
    void keepsNameMatchWhenDeviceNameEqualsScreenName();
    void pairsRemainingScreensByArrangement();
    void leavesUnmatchedMonitorWithoutScreen();
};

void MonitorLayoutTest::keepsNameMatchWhenDeviceNameEqualsScreenName()
{
    // 本机双屏（1920×1200@125% + 3840×2160@200%）的真实快照：
    // Win32 设备名恰好等于 Qt 屏幕名；旧实现用 displayName == deviceName
    // 判断“未匹配”，会把已匹配的显示器再拿去和副屏交换。
    const QVector<detail::RawMonitorInfo> monitors{
        {QStringLiteral("\\\\.\\DISPLAY1"), QRect(0, 0, 1920, 1200), true},
        {QStringLiteral("\\\\.\\DISPLAY2"), QRect(1920, -247, 3840, 2160), false}};
    const QVector<detail::ScreenInfo> screens{
        {QStringLiteral("\\\\.\\DISPLAY1"), QRect(0, 0, 1536, 960), 1.25},
        {QStringLiteral("DELL S2725QS"), QRect(1920, -247, 1920, 1080), 2.0}};

    const QVector<int> matches = detail::matchScreens(monitors, screens, 0);
    QCOMPARE(matches.size(), 2);
    QCOMPARE(matches.at(0), 0);  // 主屏必须保持与 DISPLAY1 配对
    QCOMPARE(matches.at(1), 1);  // 副屏补配对到 DELL 屏幕
}

void MonitorLayoutTest::pairsRemainingScreensByArrangement()
{
    // 名字全部对不上时：主显示器配主屏，剩余按左→右顺序配对。
    const QVector<detail::RawMonitorInfo> monitors{
        {QStringLiteral("DEV-A"), QRect(0, 0, 1920, 1080), true},
        {QStringLiteral("DEV-B"), QRect(1920, 0, 2560, 1440), false}};
    const QVector<detail::ScreenInfo> screens{
        {QStringLiteral("Primary"), QRect(0, 0, 1920, 1080), 1.0},
        {QStringLiteral("Secondary"), QRect(1920, 0, 1280, 720), 2.0}};

    const QVector<int> matches = detail::matchScreens(monitors, screens, 0);
    QCOMPARE(matches.size(), 2);
    QCOMPARE(matches.at(0), 0);
    QCOMPARE(matches.at(1), 1);
}

void MonitorLayoutTest::leavesUnmatchedMonitorWithoutScreen()
{
    // 屏幕比显示器少时不能越界/重复配对，未配对显示器保持 -1。
    const QVector<detail::RawMonitorInfo> monitors{
        {QStringLiteral("DEV-A"), QRect(0, 0, 1920, 1080), true},
        {QStringLiteral("DEV-B"), QRect(1920, 0, 1920, 1080), false},
        {QStringLiteral("DEV-C"), QRect(3840, 0, 1920, 1080), false}};
    const QVector<detail::ScreenInfo> screens{
        {QStringLiteral("Primary"), QRect(0, 0, 1920, 1080), 1.0},
        {QStringLiteral("DEV-B"), QRect(1920, 0, 1920, 1080), 1.0}};

    const QVector<int> matches = detail::matchScreens(monitors, screens, 0);
    QCOMPARE(matches.size(), 3);
    QCOMPARE(matches.at(0), 0);
    QCOMPARE(matches.at(1), 1);
    QCOMPARE(matches.at(2), -1);
}

QTEST_GUILESS_MAIN(MonitorLayoutTest)

#include "test_monitor_layout.moc"
