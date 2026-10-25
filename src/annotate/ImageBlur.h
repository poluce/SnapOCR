#pragma once

#include <QImage>

namespace snapocr::annotate {

// 快速高斯近似模糊（三次盒式模糊），用于模糊笔刷（F-ANN-16）。
// radius <= 0 或空图时原样返回；保留原图格式（统一转成 ARGB32 处理）。
QImage gaussianBlur(const QImage& source, int radius);

}  // namespace snapocr::annotate
