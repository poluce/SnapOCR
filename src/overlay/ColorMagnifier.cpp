#include "overlay/ColorMagnifier.h"

#include <QPainter>
#include <QPainterPath>

namespace snapocr::overlay {
namespace {

constexpr int kCornerRadius = 8;

const QColor kBackground(0xFF, 0xFF, 0xFF);
const QColor kBorder(0xC9, 0xCD, 0xD4);
const QColor kText(0x2B, 0x2F, 0x36);
const QColor kMutedText(0x7A, 0x81, 0x8B);
const QColor kGridLine(0xEE, 0xF0, 0xF4);
const QColor kOutOfBounds(0xF4, 0xF5, 0xF7);

}  // namespace

QColor ColorMagnifier::sampleColor(const QImage& image, const QPoint& point)
{
    if (image.isNull()) {
        return Qt::black;
    }
    const int x = qBound(0, point.x(), image.width() - 1);
    const int y = qBound(0, point.y(), image.height() - 1);
    return image.pixelColor(x, y);
}

QString ColorMagnifier::hexText(const QColor& color)
{
    return color.name(QColor::HexRgb).toUpper();
}

QString ColorMagnifier::rgbText(const QColor& color)
{
    return QStringLiteral("RGB %1, %2, %3")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue());
}

QSize ColorMagnifier::panelSize()
{
    const int gridSide = kGridPixels * kCellSize;
    return QSize(gridSide + kPadding * 2, gridSide + kInfoHeight + kPadding * 2);
}

QRectF ColorMagnifier::panelRect(const QPoint& localLogicalCursor, const QSize& parentSize)
{
    const QSize size = panelSize();
    int x = localLogicalCursor.x() + kCursorGap;
    int y = localLogicalCursor.y() + kCursorGap;
    if (x + size.width() > parentSize.width()) {
        x = localLogicalCursor.x() - size.width() - kCursorGap;
    }
    if (y + size.height() > parentSize.height()) {
        y = localLogicalCursor.y() - size.height() - kCursorGap;
    }
    x = qBound(0, x, qMax(0, parentSize.width() - size.width()));
    y = qBound(0, y, qMax(0, parentSize.height() - size.height()));
    return QRectF(QPointF(x, y), QSizeF(size));
}

void ColorMagnifier::paint(QPainter& painter, const QRectF& panel, const QImage& frameImage,
                           const QPoint& samplePoint)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(panel.adjusted(0.5, 0.5, -0.5, -0.5), kCornerRadius, kCornerRadius);
    painter.fillPath(path, kBackground);
    painter.setPen(QPen(kBorder, 1));
    painter.drawPath(path);

    const int gridSide = kGridPixels * kCellSize;
    const QRectF gridRect(panel.left() + kPadding, panel.top() + kPadding, gridSide, gridSide);
    const int half = kGridPixels / 2;

    for (int row = 0; row < kGridPixels; ++row) {
        for (int col = 0; col < kGridPixels; ++col) {
            const QPoint sample(samplePoint.x() - half + col, samplePoint.y() - half + row);
            const bool inside = !frameImage.isNull() && sample.x() >= 0 && sample.y() >= 0
                                && sample.x() < frameImage.width()
                                && sample.y() < frameImage.height();
            const QRectF cell(gridRect.left() + col * kCellSize, gridRect.top() + row * kCellSize,
                              kCellSize, kCellSize);
            painter.fillRect(cell.adjusted(0.5, 0.5, -0.5, -0.5),
                             inside ? sampleColor(frameImage, sample) : kOutOfBounds);
        }
    }

    painter.setPen(QPen(kGridLine, 1));
    for (int i = 0; i <= kGridPixels; ++i) {
        const qreal offset = i * kCellSize;
        painter.drawLine(QPointF(gridRect.left() + offset, gridRect.top()),
                         QPointF(gridRect.left() + offset, gridRect.bottom()));
        painter.drawLine(QPointF(gridRect.left(), gridRect.top() + offset),
                         QPointF(gridRect.right(), gridRect.top() + offset));
    }

    const QRectF centerCell(gridRect.left() + half * kCellSize, gridRect.top() + half * kCellSize,
                            kCellSize, kCellSize);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::white, 1.5));
    painter.drawRect(centerCell.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.setPen(QPen(Qt::black, 1.0));
    painter.drawRect(centerCell.adjusted(1.5, 1.5, -1.5, -1.5));

    const QColor color = sampleColor(frameImage, samplePoint);
    const QRectF infoRect(panel.left() + kPadding, gridRect.bottom() + 6, gridSide,
                          kInfoHeight - 8);
    const QRectF swatch(infoRect.left(), infoRect.top() + 1, 14, 14);
    painter.setPen(QPen(kBorder, 1));
    painter.setBrush(color);
    painter.drawRoundedRect(swatch, 3, 3);

    QFont mono = painter.font();
    mono.setPixelSize(12);
    mono.setBold(true);
    painter.setFont(mono);
    painter.setPen(kText);
    painter.drawText(QRectF(swatch.right() + 8, infoRect.top(), infoRect.width() - 22, 16),
                     Qt::AlignLeft | Qt::AlignVCenter, hexText(color));

    QFont small = painter.font();
    small.setPixelSize(11);
    small.setBold(false);
    painter.setFont(small);
    painter.setPen(kMutedText);
    painter.drawText(QRectF(infoRect.left(), infoRect.top() + 16, infoRect.width(), 14),
                     Qt::AlignLeft | Qt::AlignVCenter, rgbText(color));
    painter.restore();
}

}  // namespace snapocr::overlay
