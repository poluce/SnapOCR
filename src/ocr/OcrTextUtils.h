#pragma once

#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>

#include "ocr/OcrTypes.h"

namespace snapocr::ocr {

// 默认切片重叠比例（占切片边长的百分比）。
constexpr int kDefaultSliceOverlapPercent = 5;

// 规划大图切片（纯函数，便于单测）。
// Windows.Media.Ocr 对输入图像有 MaxImageDimension 限制，超过时必须切成若干块分别识别。
// 约定：
// - imageSize 无效（空）时返回空列表；
// - maxDimension <= 0 表示不限制尺寸，返回覆盖整图的一块；
// - 每个切片的宽高都不超过 maxDimension；
// - 相邻切片保留约 overlapPercent% 的重叠，便于 mergeDuplicateLines() 去掉重复行；
// - 切片按“先行后列”排列，覆盖整张图像且不越界。
QVector<QRect> planSlices(const QSize& imageSize,
                          int maxDimension,
                          int overlapPercent = kDefaultSliceOverlapPercent);

// 合并多块识别的行：文本相同、处于同一行且水平范围重叠的行只保留一次（先到先得）。
// 用于大图切片后重叠区域产生的重复行；返回结果按（上边界、左边界）排序，便于阅读顺序。
QVector<OcrLine> mergeDuplicateLines(const QVector<OcrLine>& lines);

// 整理 OCR 文本（需求 F-OCR-08 的基础能力）：
// - 去掉行首尾空白，连续空白压缩为一个空格，删除汉字（含全角标点）之间的多余空格；
// - 段落内的排版换行自动拼接：西文单词之间补一个空格，中文之间直接相连；
// - 空行、以句末标点（。！？…!? 以及英文句点）结尾的行视为段落边界，保留换行。
QString normalizeText(const QString& rawText);

}  // namespace snapocr::ocr
