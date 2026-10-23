#include "capture/CoordinateMap.h"

#include <QtMath>

namespace snapocr::capture {

QPointF localLogicalToPhysical(const MonitorInfo& monitor, const QPointF& localLogical)
{
    const qreal dpr = monitor.devicePixelRatio > 0.0 ? monitor.devicePixelRatio : 1.0;
    return QPointF(monitor.physicalGeometry.x() + localLogical.x() * dpr,
                   monitor.physicalGeometry.y() + localLogical.y() * dpr);
}

QPoint localLogicalToPhysicalPoint(const MonitorInfo& monitor, const QPointF& localLogical)
{
    const QPointF physical = localLogicalToPhysical(monitor, localLogical);
    return QPoint(qRound(physical.x()), qRound(physical.y()));
}

QPointF physicalToLocalLogical(const MonitorInfo& monitor, const QPointF& physical)
{
    const qreal dpr = monitor.devicePixelRatio > 0.0 ? monitor.devicePixelRatio : 1.0;
    return QPointF((physical.x() - monitor.physicalGeometry.x()) / dpr,
                   (physical.y() - monitor.physicalGeometry.y()) / dpr);
}

QRectF physicalRectToLocalLogical(const MonitorInfo& monitor, const QRectF& physicalRect)
{
    const qreal dpr = monitor.devicePixelRatio > 0.0 ? monitor.devicePixelRatio : 1.0;
    return QRectF((physicalRect.x() - monitor.physicalGeometry.x()) / dpr,
                  (physicalRect.y() - monitor.physicalGeometry.y()) / dpr,
                  physicalRect.width() / dpr,
                  physicalRect.height() / dpr);
}

QRect physicalRectToLocalLogicalRect(const MonitorInfo& monitor, const QRect& physicalRect)
{
    const QRectF local = physicalRectToLocalLogical(monitor, QRectF(physicalRect));
    return local.toAlignedRect();
}

QRectF localLogicalRectToPhysical(const MonitorInfo& monitor, const QRectF& localLogicalRect)
{
    const QPointF topLeft = localLogicalToPhysical(monitor, localLogicalRect.topLeft());
    const qreal dpr = monitor.devicePixelRatio > 0.0 ? monitor.devicePixelRatio : 1.0;
    return QRectF(topLeft, QSizeF(localLogicalRect.width() * dpr, localLogicalRect.height() * dpr));
}

const MonitorInfo* monitorAtPhysical(const QVector<MonitorInfo>& monitors, const QPoint& physical)
{
    for (const MonitorInfo& monitor : monitors) {
        if (monitor.physicalGeometry.contains(physical)) {
            return &monitor;
        }
    }
    return nullptr;
}

const MonitorInfo* monitorAtGlobalLogical(const QVector<MonitorInfo>& monitors,
                                          const QPointF& globalLogical)
{
    const QPoint probe(qRound(globalLogical.x()), qRound(globalLogical.y()));
    for (const MonitorInfo& monitor : monitors) {
        if (monitor.logicalGeometry.contains(probe)) {
            return &monitor;
        }
    }
    return nullptr;
}

QPointF globalLogicalToPhysical(const QVector<MonitorInfo>& monitors, const QPointF& globalLogical)
{
    const MonitorInfo* monitor = monitorAtGlobalLogical(monitors, globalLogical);
    if (monitor == nullptr) {
        // 落在所有显示器之外（拖出边界）：用最近的显示器外推，避免返回 (0,0) 抖动。
        if (monitors.isEmpty()) {
            return globalLogical;
        }
        monitor = &monitors.first();
    }
    const QPointF local = globalLogical - QPointF(monitor->logicalGeometry.topLeft());
    return localLogicalToPhysical(*monitor, local);
}

QPoint globalLogicalToPhysicalPoint(const QVector<MonitorInfo>& monitors,
                                    const QPointF& globalLogical)
{
    const QPointF physical = globalLogicalToPhysical(monitors, globalLogical);
    return QPoint(qRound(physical.x()), qRound(physical.y()));
}

QRectF physicalRectToGlobalLogical(const QVector<MonitorInfo>& monitors,
                                   const QRectF& physicalRect)
{
    const MonitorInfo* monitor =
        monitorAtPhysical(monitors, physicalRect.topLeft().toPoint());
    if (monitor == nullptr) {
        return physicalRect;
    }
    const QRectF local = physicalRectToLocalLogical(*monitor, physicalRect);
    return local.translated(monitor->logicalGeometry.topLeft());
}

}  // namespace snapocr::capture
