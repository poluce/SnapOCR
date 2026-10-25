#include "ui/ToolIcons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include <cmath>

namespace snapocr::ui {
namespace {

constexpr int kIconSize = 20;

// QIcon 里放入多档【物理尺寸】的 pixmap（不做 devicePixelRatio 标记）。
// 踩过的两个坑：
//   1) 只放 20x20 DPR=1 → 200% 下被放大，发虚；
//   2) 放带 DPR 标记的图 → Qt 取到 40x40 条目却按逻辑 20 绘制，只剩左上角碎片。
// 现在每档都是 DPR=1 的实际像素尺寸，Qt 按“目标物理尺寸”精确匹配：
//   dpr1 → 20x20，dpr2 → 40x40，dpr2.5 → 60x60 缩到 50，依次类推。
constexpr int kIconScales[] = {1, 2, 3, 4};

QColor g_inkColor = QColor(0x33, 0x33, 0x33);

QColor inkColor()
{
    return g_inkColor;
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

// 绘制函数在 20x20 的“逻辑图标空间”里作画；这里按 1x..4x 各渲染一张，
// 全部加入同一个 QIcon，交给 Qt 按屏幕缩放挑选。
QIcon renderIcon(void (*draw)(QPainter&))
{
    QIcon icon;
    for (int scale : kIconScales) {
        QPixmap pixmap(kIconSize * scale, kIconSize * scale);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        configurePainter(painter);
        painter.scale(scale, scale);
        draw(painter);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

// ---- 各图标的绘制体（沿用原有 20x20 坐标） ----

void drawSelect(QPainter& painter)
{
    QPolygonF arrow;
    arrow << QPointF(6, 3) << QPointF(6, 15) << QPointF(9.5, 11.5) << QPointF(12, 17)
          << QPointF(14, 16) << QPointF(11.5, 10.5) << QPointF(16, 10.5);
    painter.setBrush(QBrush(inkColor()));
    painter.drawPolygon(arrow);
}

void drawTool(QPainter& painter, annotate::AnnotationKind kind)
{
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
        const QColor highlight(0xFF, 0xD5, 0x4F, 170);
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
}

void drawAction(QPainter& painter, const QString& action)
{
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
    } else if (action == QLatin1String("camera")) {
        painter.drawRoundedRect(QRectF(2.5, 6.5, 15, 10), 2.5, 2.5);
        painter.drawEllipse(QRectF(7.5, 9, 5, 5));
        painter.drawLine(QPointF(6.5, 6.5), QPointF(8, 4));
        painter.drawLine(QPointF(8, 4), QPointF(12, 4));
        painter.drawLine(QPointF(12, 4), QPointF(13.5, 6.5));
    } else if (action == QLatin1String("history")) {
        // 时钟：圆 + 指针 + 逆时针箭头
        painter.drawEllipse(QPointF(10, 10.5), 7.0, 7.0);
        painter.drawLine(QPointF(10, 6.5), QPointF(10, 10.5));
        painter.drawLine(QPointF(10, 10.5), QPointF(13, 12));
        painter.drawArc(QRectF(2.5, 3.0, 5.0, 5.0), 30 * 16, 120 * 16);
    } else if (action == QLatin1String("pin")) {
        // 图钉：斜向针身 + 圆头 + 底部针尖
        painter.drawLine(QPointF(13, 3), QPointF(7, 9));
        painter.drawLine(QPointF(9, 2), QPointF(15, 8));
        painter.drawLine(QPointF(7, 9), QPointF(3, 17));
        painter.drawEllipse(QPointF(13, 1.5), 2.2, 2.2);
    } else if (action == QLatin1String("settings")) {
        // 三条带滑块的设置线条
        painter.drawLine(QPointF(3, 6), QPointF(17, 6));
        painter.drawLine(QPointF(3, 12), QPointF(17, 12));
        painter.drawLine(QPointF(3, 18), QPointF(17, 18));
        painter.setBrush(QBrush(inkColor()));
        painter.drawEllipse(QPointF(7, 6), 1.9, 1.9);
        painter.drawEllipse(QPointF(13, 12), 1.9, 1.9);
        painter.drawEllipse(QPointF(6, 18), 1.9, 1.9);
    } else if (action == QLatin1String("textedit")) {
        painter.drawRect(QRectF(4, 5, 12, 10));
        painter.drawLine(QPointF(7, 8), QPointF(13, 8));
        painter.drawLine(QPointF(7, 11), QPointF(13, 11));
    }
}

// QPainter 的回调无法携带参数，这里用文件内静态状态传递当前目标。
annotate::AnnotationKind g_pendingKind = annotate::AnnotationKind::Rectangle;
QString g_pendingAction;

void drawPendingTool(QPainter& painter)
{
    drawTool(painter, g_pendingKind);
}

void drawPendingAction(QPainter& painter)
{
    drawAction(painter, g_pendingAction);
}

}  // namespace

QIcon toolIcon(annotate::AnnotationKind kind, const QColor& color)
{
    g_inkColor = color;
    g_pendingKind = kind;
    return renderIcon(drawPendingTool);
}

QIcon actionIcon(const QString& action, const QColor& color)
{
    g_inkColor = color;
    if (action == QLatin1String("select")) {
        return renderIcon(drawSelect);
    }
    g_pendingAction = action;
    return renderIcon(drawPendingAction);
}

}  // namespace snapocr::ui
