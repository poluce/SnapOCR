#pragma once

#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace snapocr::annotate {

enum class AnnotationKind
{
    Rectangle,
    Ellipse,
    Line,
    Arrow,
    Freehand,
    Text,
    StepNumber,
    Highlight,
    Mosaic,
    Blur  // 模糊笔刷（F-ANN-16）
};

struct AnnotationStyle
{
    QColor color = QColor(QStringLiteral("#FF3B30"));
    int width = 3;            // 描边宽度（图像像素）
    int fontSize = 20;        // 文字字号（图像像素）
    int mosaicBlockSize = 12; // 马赛克块大小（图像像素）
    bool filled = false;      // 矩形/椭圆是否填充（F-ANN-18）
    bool dashed = false;      // 描边是否使用虚线（F-ANN-18）
    int blurRadius = 10;      // 模糊笔刷强度（像素，F-ANN-16）

    bool operator==(const AnnotationStyle& other) const = default;
};

// 图元坐标一律使用**图像像素**（选区裁剪后的图像，左上角为原点）。
// Text 使用 points[0] 作为文本框左上角；points[1] 由渲染时按字体度量计算，不存储。
struct AnnotationItem
{
    AnnotationKind kind = AnnotationKind::Rectangle;
    AnnotationStyle style;
    QVector<QPointF> points;
    QString text;
    int stepNumber = 0;

    bool operator==(const AnnotationItem& other) const = default;
};

// 计算图元包围盒（图像像素）。text 图元需要字体度量，故使用与渲染一致的字体。
QRectF computeBounds(const AnnotationItem& item);

// 用 QPainter 绘制单个图元；baseImage 用于马赛克取像素，blurredBase 用于模糊笔刷
// （两者都可为空：为空时退化为半透明灰块，保证渲染不失败）。
void paintItem(QPainter& painter, const AnnotationItem& item, const QImage& baseImage,
               const QImage& blurredBase = QImage());

// 构造文字图元使用的字体（像素单位，与 DPI 无关）。
QFont annotationFont(const AnnotationStyle& style);

}  // namespace snapocr::annotate
