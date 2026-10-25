#pragma once

#include <QRect>
#include <QSize>
#include <QString>

#include "ocr/OcrTypes.h"

namespace snapocr::ocr {

// 识别失败/无结果时的可执行提示（F-OCR-09）。
// 输入都是纯数据，便于单测；不做任何 UI 操作。
//
// selectionSize        选区物理像素尺寸
// touchesScreenEdge    选区是否贴到了虚拟屏边缘（贴边可能裁掉文字）
QString describeOcrProblem(const OcrResult& result, const QSize& selectionSize,
                           bool touchesScreenEdge);

// 选区是否贴到虚拟桌面边缘（容差 1px）。
bool selectionTouchesScreenEdge(const QRect& selectionPhysical,
                                const QRect& virtualDesktopPhysical);

}  // namespace snapocr::ocr
