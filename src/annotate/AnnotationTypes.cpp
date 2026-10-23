#include "annotate/AnnotationTypes.h"

#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace snapocr::annotate {
namespace {

QRectF pointsBounds(const QVector<QPointF>& points, qreal lineWidth)
{
    if (points.isEmpty()) {
        return {};
    }
    qreal minX = points.first().x();
    qreal minY = points.first().y();
    qreal maxX = minX;
    qreal maxY = minY;
    for (const QPointF& point : points) {
        minX = std::min(minX, point.x());
        minY = std::min(minY, point.y());
        maxX = std::max(maxX, point.x());
        maxY = std::max(maxY, point.y());
    }
    const qreal pad = std::max<qreal>(1.0, lineWidth / 2.0 + 1.0);
    return QRectF(QPointF(minX - pad, minY - pad), QPointF(maxX + pad, maxY + pad));
}

QPointF arrowTip(const AnnotationItem& item)
{
    if (item.points.size() < 2) {
        return item.points.value(0);
    }
    return item.points.last();
}

}  // namespace

QFont annotationFont(const AnnotationStyle& style)
{
    QFont font;
    font.setPixelSize(std::max(6, style.fontSize));
    font.setBold(false);
    return font;
}

QRectF computeBounds(const AnnotationItem& item)
{
    switch (item.kind) {
    case AnnotationKind::Rectangle:
    case AnnotationKind::Ellipse:
    case AnnotationKind::Highlight:
    case AnnotationKind::Mosaic: {
        if (item.points.size() < 2) {
            return {};
        }
        const QRectF rect =
            QRectF(item.points[0], item.points[1]).normalized().adjusted(
                -item.style.width / 2.0, -item.style.width / 2.0, item.style.width / 2.0,
                item.style.width / 2.0);
        return rect;
    }
    case AnnotationKind::Line:
    case AnnotationKind::Arrow:
        return pointsBounds(item.points, item.style.width + 6);
    case AnnotationKind::Freehand:
        return pointsBounds(item.points, item.style.width);
    case AnnotationKind::Text: {
        if (item.points.isEmpty() || item.text.isEmpty()) {
            return {};
        }
        const QFontMetricsF metrics(annotationFont(item.style));
        const QRectF textRect = metrics.boundingRect(QRectF(), Qt::AlignLeft | Qt::AlignTop,
                                                     item.text);
        return QRectF(item.points.first(), textRect.size());
    }
    case AnnotationKind::StepNumber: {
        if (item.points.isEmpty()) {
            return {};
        }
        const qreal radius = std::max<qreal>(10.0, item.style.fontSize * 0.9);
        return QRectF(item.points.first().x() - radius, item.points.first().y() - radius,
                      radius * 2, radius * 2);
    }
    }
    return {};
}

void paintItem(QPainter& painter, const AnnotationItem& item, const QImage& baseImage)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, item.kind != AnnotationKind::Mosaic);

    QPen pen(item.style.color);
    pen.setWidthF(std::max(1.0, static_cast<double>(item.style.width)));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (item.kind) {
    case AnnotationKind::Rectangle: {
        if (item.points.size() < 2) {
            break;
        }
        const QRectF rect = QRectF(item.points[0], item.points[1]).normalized();
        painter.setBrush(item.style.filled ? QBrush(item.style.color) : QBrush(Qt::NoBrush));
        painter.drawRect(rect);
        break;
    }
    case AnnotationKind::Ellipse: {
        if (item.points.size() < 2) {
            break;
        }
        const QRectF rect = QRectF(item.points[0], item.points[1]).normalized();
        painter.setBrush(item.style.filled ? QBrush(item.style.color) : QBrush(Qt::NoBrush));
        painter.drawEllipse(rect);
        break;
    }
    case AnnotationKind::Line: {
        if (item.points.size() < 2) {
            break;
        }
        painter.drawLine(item.points.first(), item.points.last());
        break;
    }
    case AnnotationKind::Arrow: {
        if (item.points.size() < 2) {
            break;
        }
        const QPointF start = item.points.first();
        const QPointF tip = arrowTip(item);
        painter.drawLine(start, tip);
        const double angle = std::atan2(tip.y() - start.y(), tip.x() - start.x());
        const double length = std::max(10.0, static_cast<double>(item.style.width) * 4.0);
        const double spread = M_PI / 7.0;
        QPolygonF head;
        head << tip
             << QPointF(tip.x() - length * std::cos(angle - spread),
                        tip.y() - length * std::sin(angle - spread))
             << QPointF(tip.x() - length * std::cos(angle + spread),
                        tip.y() - length * std::sin(angle + spread));
        painter.setBrush(QBrush(item.style.color));
        painter.drawPolygon(head);
        break;
    }
    case AnnotationKind::Freehand: {
        if (item.points.size() < 2) {
            break;
        }
        QPainterPath path(item.points.first());
        for (int i = 1; i < item.points.size(); ++i) {
            path.lineTo(item.points[i]);
        }
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
        break;
    }
    case AnnotationKind::Text: {
        if (item.points.isEmpty() || item.text.isEmpty()) {
            break;
        }
        painter.setFont(annotationFont(item.style));
        // 轻描边提升在复杂背景上的可读性。
        painter.setPen(QPen(QColor(0, 0, 0, 140), std::max(1.0, item.style.width / 2.0)));
        QFontMetricsF metrics(annotationFont(item.style));
        const QRectF textRect = metrics.boundingRect(QRectF(), Qt::AlignLeft | Qt::AlignTop,
                                                     item.text);
        const QRectF target(item.points.first(), textRect.size());
        painter.drawText(target.translated(1, 1), Qt::AlignLeft | Qt::AlignTop, item.text);
        painter.setPen(QPen(item.style.color));
        painter.drawText(target, Qt::AlignLeft | Qt::AlignTop, item.text);
        break;
    }
    case AnnotationKind::StepNumber: {
        if (item.points.isEmpty()) {
            break;
        }
        const qreal radius = std::max<qreal>(10.0, item.style.fontSize * 0.9);
        const QPointF center = item.points.first();
        painter.setBrush(QBrush(item.style.color));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(center, radius, radius);

        QFont numberFont = annotationFont(item.style);
        numberFont.setPixelSize(static_cast<int>(radius * 1.25));
        painter.setFont(numberFont);
        painter.setPen(QPen(Qt::white));
        painter.drawText(QRectF(center.x() - radius, center.y() - radius, radius * 2, radius * 2),
                         Qt::AlignCenter, QString::number(item.stepNumber));
        break;
    }
    case AnnotationKind::Highlight: {
        if (item.points.size() < 2) {
            break;
        }
        QColor highlight = item.style.color;
        highlight.setAlpha(70);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(highlight));
        painter.drawRect(QRectF(item.points[0], item.points[1]).normalized());
        break;
    }
    case AnnotationKind::Mosaic: {
        if (item.points.size() < 2) {
            break;
        }
        const QRectF rect = QRectF(item.points[0], item.points[1]).normalized();
        const QRect sourceRect = rect.toAlignedRect().intersected(baseImage.rect());
        if (sourceRect.isEmpty()) {
            painter.fillRect(rect, QColor(128, 128, 128, 180));
            break;
        }
        const int block = std::max(4, item.style.mosaicBlockSize);
        const QSize smallSize(std::max(1, sourceRect.width() / block),
                              std::max(1, sourceRect.height() / block));
        const QImage region = baseImage.copy(sourceRect);
        const QImage pixelated =
            region.scaled(smallSize, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                .scaled(sourceRect.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(sourceRect, pixelated);
        break;
    }
    }

    painter.restore();
}

}  // namespace snapocr::annotate
