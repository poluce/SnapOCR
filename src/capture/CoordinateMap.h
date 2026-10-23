#pragma once

#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "capture/CaptureTypes.h"

namespace snapocr::capture {

// 坐标约定：
// - 覆盖层窗口的局部逻辑坐标：以该显示器左上角为 (0,0)，单位是 Qt 逻辑像素；
// - 物理坐标：虚拟桌面物理像素（可为负），是选区、裁剪、导出的唯一权威坐标。
// 这里仅做逐屏换算，禁止用全桌面统一缩放系数（混合 DPI 会错位）。

QPointF localLogicalToPhysical(const MonitorInfo& monitor, const QPointF& localLogical);
QPoint localLogicalToPhysicalPoint(const MonitorInfo& monitor, const QPointF& localLogical);

QPointF physicalToLocalLogical(const MonitorInfo& monitor, const QPointF& physical);
QRectF physicalRectToLocalLogical(const MonitorInfo& monitor, const QRectF& physicalRect);
QRect physicalRectToLocalLogicalRect(const MonitorInfo& monitor, const QRect& physicalRect);

QRectF localLogicalRectToPhysical(const MonitorInfo& monitor, const QRectF& localLogicalRect);

// 查找包含给定物理坐标的显示器；找不到返回 nullptr。
const MonitorInfo* monitorAtPhysical(const QVector<MonitorInfo>& monitors, const QPoint& physical);

// 查找包含给定全局逻辑坐标的显示器。
const MonitorInfo* monitorAtGlobalLogical(const QVector<MonitorInfo>& monitors,
                                          const QPointF& globalLogical);

// 全局逻辑坐标（Qt 虚拟桌面）→ 物理坐标。用于鼠标事件换算。
QPointF globalLogicalToPhysical(const QVector<MonitorInfo>& monitors, const QPointF& globalLogical);
QPoint globalLogicalToPhysicalPoint(const QVector<MonitorInfo>& monitors, const QPointF& globalLogical);

// 物理矩形 → 全局逻辑矩形（用于覆盖层/编辑器/工具条窗口定位）。
// 以物理矩形左上角所在显示器的 DPR 为准；跨屏时尺寸为近似值，但内容导出始终按物理像素。
QRectF physicalRectToGlobalLogical(const QVector<MonitorInfo>& monitors,
                                   const QRectF& physicalRect);

}  // namespace snapocr::capture
