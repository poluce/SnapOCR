#include "ui/ToggleSwitch.h"

#include <QPainter>
#include <QPropertyAnimation>

namespace snapocr::ui {
namespace {

constexpr int kWidth = 44;
constexpr int kHeight = 24;
constexpr int kMargin = 3;
constexpr int kDurationMs = 120;

const QColor kTrackOff(0xD5, 0xD9, 0xE0);
const QColor kTrackOffHover(0xC7, 0xCC, 0xD4);
const QColor kTrackOn(0x1F, 0x6F, 0xEB);
const QColor kTrackOnHover(0x1A, 0x5F, 0xD0);
const QColor kKnob(0xFF, 0xFF, 0xFF);

}  // namespace

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(kWidth, kHeight);

    m_animation = new QPropertyAnimation(this, "knobPosition", this);
    m_animation->setDuration(kDurationMs);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(kWidth, kHeight);
}

void ToggleSwitch::setKnobPosition(qreal position)
{
    m_knobPosition = qBound(0.0, position, 1.0);
    update();
}

void ToggleSwitch::animateTo(qreal target)
{
    m_animation->stop();
    m_animation->setStartValue(m_knobPosition);
    m_animation->setEndValue(target);
    m_animation->start();
}

void ToggleSwitch::nextCheckState()
{
    setChecked(!isChecked());
    animateTo(isChecked() ? 1.0 : 0.0);
    if (m_knobPosition <= 0.0 && isChecked()) {
        setKnobPosition(0.0);
    }
}

void ToggleSwitch::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void ToggleSwitch::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal radius = height() / 2.0;
    QColor track = isChecked() ? (m_hovered ? kTrackOnHover : kTrackOn)
                               : (m_hovered ? kTrackOffHover : kTrackOff);
    if (!isEnabled()) {
        track.setAlpha(110);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(track);
    painter.drawRoundedRect(QRectF(0, 0, width(), height()), radius, radius);

    const qreal knobRadius = radius - kMargin;
    const qreal left = radius;
    const qreal right = width() - radius;
    const qreal center = left + (right - left) * m_knobPosition;
    painter.setBrush(isEnabled() ? kKnob : QColor(0xF2, 0xF3, 0xF5));
    painter.drawEllipse(QPointF(center, height() / 2.0), knobRadius, knobRadius);

    if (hasFocus()) {
        QPen focusPen(QColor(0x1F, 0x6F, 0xEB, 90));
        focusPen.setWidth(2);
        painter.setPen(focusPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(0, 0, width() - 0.5, height() - 0.5), radius, radius);
    }
}

}  // namespace snapocr::ui
