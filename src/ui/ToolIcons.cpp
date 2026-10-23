#include "ui/ToolIcons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include <cmath>

namespace snapocr::ui {
namespace {

constexpr int kIconSize = 20;

QColor g_inkColor = QColor(0x33, 0x33, 0x33);

QColor inkColor()
{
    return g_inkColor;
}

QPixmap makePixmap()
{
    QPixmap pixmap(kIconSize, kIconSize);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

void configurePainter(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(inkColor());
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
}

QIcon finish(const QPixmap& pixmap)
{
    return QIcon(pixmap);
}

QIcon selectIcon()
{
    QPixmap pixmap = makePixmap();
    QPainter painter(&pixmap);
    configurePainter(painter);
    QPolygonF arrow;
    arrow << QPointF(6, 3) << QPointF(6, 15) << QPointF(9.5, 11.5) << QPointF(12, 17)
          << QPointF(14, 16) << QPointF(11.5, 10.5) << QPointF(16, 10.5);
    painter.setBrush(QBrush(inkColor()));
    painter.drawPolygon(arrow);
    painter.end();
    return finish(pixmap);
}

}  // namespace

QIcon toolIcon(annotate::AnnotationKind kind, const QColor& color)
{
    g_inkColor = color;
    QPixmap pixmap = makePixmap();
    QPainter painter(&pixmap);
    configurePainter(painter);

    switch (kind) {
    case annotate::AnnotationKind::Rectangle:
        painter.drawRect(QRectF(4, 6, 12, 9));
        break;
    case annotate::AnnotationKind::Ellipse:
        painter.drawEllipse(QRectF(4, 6, 12, 9));
        break;
    case annotate::AnnotationKind::Line:
        painter.drawLine(QPointF(4, 16), QPointF(16, 4));
        break;
    case annotate::AnnotationKind::Arrow: {
        painter.drawLine(QPointF(4, 16), QPointF(15, 5));
        QPolygonF head;
        head << QPointF(16, 4) << QPointF(11, 5) << QPointF(15, 9);
        painter.setBrush(QBrush(inkColor()));
        painter.drawPolygon(head);
        break;
    }
    case annotate::AnnotationKind::Freehand: {
        QPainterPath path(QPointF(4, 14));
        path.cubicTo(QPointF(8, 4), QPointF(12, 18), QPointF(16, 7));
        painter.drawPath(path);
        break;
    }
    case annotate::AnnotationKind::Text: {
        QFont font = painter.font();
        font.setPixelSize(14);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(QRectF(2, 2, 16, 16), Qt::AlignCenter, QStringLiteral("A"));
        break;
    }
    case annotate::AnnotationKind::StepNumber: {
        painter.drawEllipse(QRectF(3, 3, 14, 14));
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(QRectF(3, 3, 14, 14), Qt::AlignCenter, QStringLiteral("1"));
        break;
    }
    case annotate::AnnotationKind::Highlight: {
        QColor highlight(0xFF, 0xD5, 0x4F, 170);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(highlight));
        painter.drawRect(QRectF(3, 7, 14, 7));
        break;
    }
    case annotate::AnnotationKind::Mosaic: {
        painter.setPen(Qt::NoPen);
        const int block = 4;
        bool dark = true;
        for (int y = 4; y < 17; y += block) {
            for (int x = 3; x < 17; x += block) {
                painter.setBrush(QBrush(dark ? inkColor() : QColor(0xAA, 0xAA, 0xAA)));
                painter.drawRect(QRectF(x, y, block, block));
                dark = !dark;
            }
        }
        break;
    }
    }

    painter.end();
    return finish(pixmap);
}

QIcon actionIcon(const QString& action, const QColor& color)
{
    g_inkColor = color;
    if (action == QLatin1String("select")) {
        return selectIcon();
    }

    QPixmap pixmap = makePixmap();
    QPainter painter(&pixmap);
    configurePainter(painter);

    if (action == QLatin1String("undo") || action == QLatin1String("redo")) {
        const bool redo = action == QLatin1String("redo");
        painter.save();
        if (redo) {
            painter.translate(kIconSize, 0);
            painter.scale(-1, 1);
        }
        QPainterPath path(QPointF(15, 14));
        path.cubicTo(QPointF(14, 7), QPointF(8, 6), QPointF(5, 9));
        painter.drawPath(path);
        QPolygonF head;
        head << QPointF(4, 5) << QPointF(5, 10) << QPointF(9.5, 8.5);
        painter.setBrush(QBrush(inkColor()));
        painter.drawPolygon(head);
        painter.restore();
    } else if (action == QLatin1String("copy")) {
        painter.drawRect(QRectF(4, 4, 9, 11));
        painter.drawRect(QRectF(8, 7, 8, 10));
    } else if (action == QLatin1String("save")) {
        painter.drawRect(QRectF(4, 4, 12, 12));
        painter.drawLine(QPointF(7, 16), QPointF(7, 11));
        painter.drawLine(QPointF(7, 11), QPointF(13, 11));
        painter.drawLine(QPointF(13, 11), QPointF(13, 16));
    } else if (action == QLatin1String("ocr")) {
        painter.drawRect(QRectF(4, 5, 12, 10));
        QFont font = painter.font();
        font.setPixelSize(9);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(QRectF(4, 5, 12, 10), Qt::AlignCenter, QStringLiteral("T"));
    } else if (action == QLatin1String("close")) {
        painter.drawLine(QPointF(5, 5), QPointF(15, 15));
        painter.drawLine(QPointF(15, 5), QPointF(5, 15));
    } else if (action == QLatin1String("confirm")) {
        QPainterPath path(QPointF(4, 11));
        path.lineTo(QPointF(8, 15));
        path.lineTo(QPointF(16, 5));
        painter.drawPath(path);
    } else if (action == QLatin1String("textedit")) {
        painter.drawRect(QRectF(4, 5, 12, 10));
        painter.drawLine(QPointF(7, 8), QPointF(13, 8));
        painter.drawLine(QPointF(7, 11), QPointF(13, 11));
    }

    painter.end();
    return finish(pixmap);
}

}  // namespace snapocr::ui
