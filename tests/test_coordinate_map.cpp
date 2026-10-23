#include <QtTest>

#include <QRect>
#include <QVector>

#include "capture/CoordinateMap.h"

using snapocr::capture::MonitorInfo;
using snapocr::capture::localLogicalToPhysical;
using snapocr::capture::monitorAtPhysical;
using snapocr::capture::physicalToLocalLogical;
using snapocr::capture::physicalRectToLocalLogical;

namespace {

MonitorInfo makeMonitor(const QRect& physical, const QRect& logical, qreal dpr, bool primary = false)
{
    MonitorInfo monitor;
    monitor.id = QStringLiteral("test-monitor");
    monitor.primary = primary;
    monitor.physicalGeometry = physical;
    monitor.logicalGeometry = logical;
    monitor.devicePixelRatio = dpr;
    return monitor;
}

}  // namespace

class CoordinateMapTest final : public QObject
{
    Q_OBJECT

private slots:
    void mapsLocalPointToPhysicalAt200Percent();
    void mapsPhysicalPointBackToLocal();
    void handlesNegativePhysicalOrigin();
    void convertsRectBetweenSpaces();
    void findsMonitorAtPhysicalPosition();
    void handlesFractionalDprWithinTolerance();
};

void CoordinateMapTest::mapsLocalPointToPhysicalAt200Percent()
{
    const MonitorInfo monitor = makeMonitor(QRect(0, 0, 3840, 2160), QRect(0, 0, 1920, 1080), 2.0);
    const QPointF physical = localLogicalToPhysical(monitor, QPointF(10.0, 20.0));
    QCOMPARE(physical, QPointF(20.0, 40.0));
}

void CoordinateMapTest::mapsPhysicalPointBackToLocal()
{
    const MonitorInfo monitor = makeMonitor(QRect(100, 50, 2560, 1440), QRect(50, 25, 1280, 720), 2.0);
    const QPointF local = physicalToLocalLogical(monitor, QPointF(140.0, 90.0));
    QCOMPARE(local, QPointF(20.0, 20.0));
}

void CoordinateMapTest::handlesNegativePhysicalOrigin()
{
    // 副屏位于主屏左侧：物理坐标为负，Qt 逻辑坐标也可能为负。
    const MonitorInfo monitor = makeMonitor(QRect(-2560, 0, 2560, 1440), QRect(-1280, 0, 1280, 720), 2.0);
    QCOMPARE(localLogicalToPhysical(monitor, QPointF(0.0, 0.0)), QPointF(-2560.0, 0.0));
    QCOMPARE(localLogicalToPhysical(monitor, QPointF(10.0, 10.0)), QPointF(-2540.0, 20.0));
    QCOMPARE(physicalToLocalLogical(monitor, QPointF(-2540.0, 20.0)), QPointF(10.0, 10.0));
}

void CoordinateMapTest::convertsRectBetweenSpaces()
{
    const MonitorInfo monitor = makeMonitor(QRect(0, 0, 3840, 2160), QRect(0, 0, 1920, 1080), 2.0);
    const QRectF local = physicalRectToLocalLogical(monitor, QRectF(200, 100, 400, 300));
    QCOMPARE(local, QRectF(100, 50, 200, 150));
}

void CoordinateMapTest::findsMonitorAtPhysicalPosition()
{
    QVector<MonitorInfo> monitors;
    monitors.append(makeMonitor(QRect(0, 0, 3840, 2160), QRect(0, 0, 1920, 1080), 2.0, true));
    monitors.append(makeMonitor(QRect(-2560, 0, 2560, 1440), QRect(-1280, 0, 1280, 720), 2.0));

    QCOMPARE(monitorAtPhysical(monitors, QPoint(10, 10)), &monitors[0]);
    QCOMPARE(monitorAtPhysical(monitors, QPoint(-10, 10)), &monitors[1]);
    QCOMPARE(monitorAtPhysical(monitors, QPoint(-3000, 10)), nullptr);
}

void CoordinateMapTest::handlesFractionalDprWithinTolerance()
{
    const MonitorInfo monitor = makeMonitor(QRect(0, 0, 2560, 1440), QRect(0, 0, 1707, 960), 1.5);
    const QPointF physical = localLogicalToPhysical(monitor, QPointF(100.0, 50.0));
    QCOMPARE(physical, QPointF(150.0, 75.0));
    const QPointF local = physicalToLocalLogical(monitor, physical);
    QVERIFY(qAbs(local.x() - 100.0) < 0.001);
    QVERIFY(qAbs(local.y() - 50.0) < 0.001);
}

QTEST_GUILESS_MAIN(CoordinateMapTest)

#include "test_coordinate_map.moc"
