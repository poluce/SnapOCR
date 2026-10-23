#include "overlay/ToolbarTooltip.h"

#include <QGuiApplication>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

namespace snapocr::overlay {
namespace {

constexpr int kPaddingX = 8;
constexpr int kPaddingY = 5;
constexpr int kRadius = 6;

const QColor kBackground(0xFF, 0xFF, 0xFF);
const QColor kBorder(0xC9, 0xCD, 0xD4);
const QColor kText(0x2B, 0x2F, 0x36);

}  // namespace

ToolbarTooltip::ToolbarTooltip()
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                       | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);

    QFont font = this->font();
    font.setPixelSize(12);
    setFont(font);

    // 覆盖层是同级的置顶全屏窗口，被重新 raise 时会把气泡压下去；
    // 显示期间定时重新置顶（开销极小），确保提示始终可见。
    m_raiseTimer = new QTimer(this);
    m_raiseTimer->setInterval(120);
    connect(m_raiseTimer, &QTimer::timeout, this, [this]() { keepOnTop(); });
}

ToolbarTooltip* ToolbarTooltip::instance()
{
    // 进程内单例，避免每次提示都创建窗口。
    static ToolbarTooltip* tooltip = new ToolbarTooltip;
    return tooltip;
}

void ToolbarTooltip::showFor(QWidget* anchor, const QString& text)
{
    if (anchor == nullptr || text.isEmpty()) {
        return;
    }

    m_text = text;
    QFontMetrics metrics(font());
    const int textWidth = metrics.horizontalAdvance(text);
    const int textHeight = metrics.height();
    resize(textWidth + kPaddingX * 2, textHeight + kPaddingY * 2);

    // 默认放在锚点下方居中；下方空间不足则放上方。
    const QRect anchorRect(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
    QScreen* screen = anchor->screen() != nullptr ? anchor->screen() : QGuiApplication::primaryScreen();
    const QRect available = screen != nullptr ? screen->availableGeometry()
                                              : QRect(anchorRect.topLeft(), QSize(1920, 1080));

    int x = anchorRect.center().x() - width() / 2;
    int y = anchorRect.bottom() + 6;
    if (y + height() > available.bottom()) {
        y = anchorRect.top() - height() - 6;
    }
    if (y < available.top()) {
        y = anchorRect.bottom() + 6;
    }
    if (x + width() > available.right()) {
        x = available.right() - width() - 4;
    }
    if (x < available.left()) {
        x = available.left() + 4;
    }

    move(x, y);
    setWindowOpacity(1.0);
    show();
    raise();
    update();
    if (m_raiseTimer != nullptr) {
        m_raiseTimer->start();
    }
}

void ToolbarTooltip::hideNow()
{
    if (m_raiseTimer != nullptr) {
        m_raiseTimer->stop();
    }
    hide();
}

void ToolbarTooltip::keepOnTop()
{
    if (!isVisible()) {
        if (m_raiseTimer != nullptr) {
            m_raiseTimer->stop();
        }
        return;
    }
    raise();
}

void ToolbarTooltip::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
    painter.fillPath(path, kBackground);
    painter.setPen(QPen(kBorder, 1));
    painter.drawPath(path);

    painter.setPen(kText);
    painter.drawText(rect().adjusted(kPaddingX, kPaddingY, -kPaddingX, -kPaddingY),
                     Qt::AlignLeft | Qt::AlignVCenter, m_text);
}

}  // namespace snapocr::overlay
