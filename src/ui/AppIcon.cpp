#include "ui/AppIcon.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

namespace snapocr::ui {
namespace {

QPixmap drawIcon(int size, bool forTray)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal scale = size / 32.0;
    painter.scale(scale, scale);

    // 圆角底板
    const QColor background = QColor(0x1F, 0x6F, 0xEB);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF(1, 1, 30, 30), 7, 7);

    // 取景框四角
    QPen framePen(Qt::white);
    framePen.setWidthF(forTray ? 2.6 : 2.2);
    framePen.setCapStyle(Qt::RoundCap);
    painter.setPen(framePen);
    painter.setBrush(Qt::NoBrush);
    const qreal margin = 7;
    const qreal arm = 5;
    // 左上
    painter.drawLine(QPointF(margin, margin + arm), QPointF(margin, margin));
    painter.drawLine(QPointF(margin, margin), QPointF(margin + arm, margin));
    // 右上
    painter.drawLine(QPointF(32 - margin - arm, margin), QPointF(32 - margin, margin));
    painter.drawLine(QPointF(32 - margin, margin), QPointF(32 - margin, margin + arm));
    // 左下
    painter.drawLine(QPointF(margin, 32 - margin - arm), QPointF(margin, 32 - margin));
    painter.drawLine(QPointF(margin, 32 - margin), QPointF(margin + arm, 32 - margin));
    // 右下
    painter.drawLine(QPointF(32 - margin - arm, 32 - margin), QPointF(32 - margin, 32 - margin));
    painter.drawLine(QPointF(32 - margin, 32 - margin - arm), QPointF(32 - margin, 32 - margin));

    // 中间文字标记
    QFont font = painter.font();
    font.setPixelSize(13);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QPen(Qt::white));
    painter.drawText(QRectF(0, 0, 32, 32), Qt::AlignCenter, QStringLiteral("T"));

    painter.end();
    return pixmap;
}

}  // namespace

QIcon appIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        icon.addPixmap(drawIcon(size, false));
    }
    return icon;
}

QIcon trayIcon()
{
    QIcon icon;
    for (int size : {16, 20, 24, 32, 48}) {
        icon.addPixmap(drawIcon(size, true));
    }
    return icon;
}

}  // namespace snapocr::ui
