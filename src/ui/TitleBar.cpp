#include "ui/TitleBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QWindow>

#include "ui/AppIcon.h"

namespace snapocr::ui {

TitleBar::TitleBar(const QString& title, QWidget* parent, bool showMinimize, bool showClose)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("titleBar"));
    setFixedHeight(46);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 8, 0);
    layout->setSpacing(10);

    auto* iconLabel = new QLabel(this);
    iconLabel->setPixmap(windowIcon().pixmap(18, 18));
    iconLabel->setFixedSize(18, 18);
    if (iconLabel->pixmap().isNull()) {
        iconLabel->setPixmap(appIcon().pixmap(18, 18));
    }
    layout->addWidget(iconLabel);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName(QStringLiteral("titleBarText"));
    layout->addWidget(m_titleLabel);
    layout->addStretch(1);

    if (showMinimize) {
        m_minimizeButton = new QToolButton(this);
        m_minimizeButton->setObjectName(QStringLiteral("titleBarButton"));
        m_minimizeButton->setText(QStringLiteral("\u2013"));
        m_minimizeButton->setToolTip(QStringLiteral("最小化"));
        m_minimizeButton->setCursor(Qt::ArrowCursor);
        m_minimizeButton->setFixedSize(32, 28);
        layout->addWidget(m_minimizeButton);
        connect(m_minimizeButton, &QToolButton::clicked, this, &TitleBar::minimizeRequested);
    }

    if (showClose) {
        m_closeButton = new QToolButton(this);
        m_closeButton->setObjectName(QStringLiteral("titleBarClose"));
        m_closeButton->setText(QStringLiteral("\u2715"));
        m_closeButton->setToolTip(QStringLiteral("关闭到托盘"));
        m_closeButton->setCursor(Qt::ArrowCursor);
        m_closeButton->setFixedSize(32, 28);
        layout->addWidget(m_closeButton);
        connect(m_closeButton, &QToolButton::clicked, this, &TitleBar::closeRequested);
    }
}

void TitleBar::setTitle(const QString& title)
{
    m_titleLabel->setText(title);
}

void TitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (QWindow* handle = window()->windowHandle()) {
            // 交给系统做拖动，高 DPI / 多屏下不会出现跟手偏移。
            handle->startSystemMove();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    // 设置窗固定大小，双击标题栏不做最大化。
    Q_UNUSED(event);
}

void TitleBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    // 顶部区域填白并保留上方圆角，底部画一条细分隔线
    const qreal radius = 12;
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), radius, radius);
    QPainterPath bottomRect;
    bottomRect.addRect(QRectF(0, height() - radius, width(), radius));
    painter.fillPath(path.united(bottomRect), QColor(0xFF, 0xFF, 0xFF));

    painter.setPen(QPen(QColor(0xE6, 0xE9, 0xEE), 1));
    painter.drawLine(0, height() - 1, width(), height() - 1);
}

}  // namespace snapocr::ui
