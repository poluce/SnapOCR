#include "overlay/SelectionModel.h"

#include <QPoint>
#include <QRect>
#include <algorithm>

namespace snapocr::overlay {

void SelectionModel::setBounds(const QRect& physicalBounds)
{
    m_bounds = physicalBounds;
}

QRect SelectionModel::rect() const
{
    if (!m_active) {
        return {};
    }
    return m_rect.normalized();
}

void SelectionModel::reset()
{
    m_active = false;
    m_mode = DragMode::None;
    m_dragHandle = Handle::None;
    m_rect = {};
    m_dragStartRect = {};
    m_dragStartPos = {};
}

QRect SelectionModel::clamped(const QRect& physicalRect) const
{
    if (m_bounds.isEmpty()) {
        return physicalRect;
    }
    return physicalRect.intersected(m_bounds);
}

void SelectionModel::beginCreate(const QPoint& physicalPos)
{
    m_active = true;
    m_mode = DragMode::Creating;
    m_dragStartPos = physicalPos;
    m_rect = QRect(physicalPos, QSize(0, 0));
}

void SelectionModel::updateCreate(const QPoint& physicalPos)
{
    if (m_mode != DragMode::Creating) {
        return;
    }
    // 显式取 min/max，避免 QRect(反向两点).normalized() 的 ±1 像素偏移。
    const int x1 = std::min(m_dragStartPos.x(), physicalPos.x());
    const int y1 = std::min(m_dragStartPos.y(), physicalPos.y());
    const int x2 = std::max(m_dragStartPos.x(), physicalPos.x());
    const int y2 = std::max(m_dragStartPos.y(), physicalPos.y());
    m_rect = clamped(QRect(QPoint(x1, y1), QPoint(x2, y2)));
}

void SelectionModel::beginMove(const QPoint& physicalPos)
{
    m_mode = DragMode::Moving;
    m_dragStartPos = physicalPos;
    m_dragStartRect = m_rect.normalized();
}

void SelectionModel::beginResize(Handle handle, const QPoint& physicalPos)
{
    m_mode = DragMode::Resizing;
    m_dragHandle = handle;
    m_dragStartPos = physicalPos;
    m_dragStartRect = m_rect.normalized();
}

void SelectionModel::updateDrag(const QPoint& physicalPos)
{
    if (m_mode == DragMode::Moving || m_mode == DragMode::Resizing) {
        applyDrag(physicalPos);
    }
}

void SelectionModel::applyDrag(const QPoint& physicalPos)
{
    const QPoint delta = physicalPos - m_dragStartPos;

    if (m_mode == DragMode::Moving) {
        QRect moved = m_dragStartRect.translated(delta);
        if (!m_bounds.isEmpty()) {
            if (moved.left() < m_bounds.left()) {
                moved.moveLeft(m_bounds.left());
            }
            if (moved.top() < m_bounds.top()) {
                moved.moveTop(m_bounds.top());
            }
            if (moved.right() > m_bounds.right()) {
                moved.moveRight(m_bounds.right());
            }
            if (moved.bottom() > m_bounds.bottom()) {
                moved.moveBottom(m_bounds.bottom());
            }
        }
        m_rect = moved;
        return;
    }

    if (m_mode != DragMode::Resizing) {
        return;
    }

    QRect resized = m_dragStartRect;
    int left = resized.left();
    int top = resized.top();
    int right = resized.right();
    int bottom = resized.bottom();

    switch (m_dragHandle) {
    case Handle::TopLeft:
        left += delta.x();
        top += delta.y();
        break;
    case Handle::Top:
        top += delta.y();
        break;
    case Handle::TopRight:
        right += delta.x();
        top += delta.y();
        break;
    case Handle::Right:
        right += delta.x();
        break;
    case Handle::BottomRight:
        right += delta.x();
        bottom += delta.y();
        break;
    case Handle::Bottom:
        bottom += delta.y();
        break;
    case Handle::BottomLeft:
        left += delta.x();
        bottom += delta.y();
        break;
    case Handle::Left:
        left += delta.x();
        break;
    default:
        return;
    }

    // 夹取到虚拟桌面边界，并保证至少 1px 宽高。
    if (!m_bounds.isEmpty()) {
        left = std::max(left, m_bounds.left());
        top = std::max(top, m_bounds.top());
        right = std::min(right, m_bounds.right());
        bottom = std::min(bottom, m_bounds.bottom());
    }
    if (right < left) {
        std::swap(left, right);
    }
    if (bottom < top) {
        std::swap(bottom, top);
    }

    m_rect = QRect(QPoint(left, top), QPoint(right, bottom)).normalized();
}

void SelectionModel::nudge(int dx, int dy)
{
    if (!m_active) {
        return;
    }
    QRect moved = m_rect.normalized().translated(dx, dy);
    if (!m_bounds.isEmpty()) {
        if (moved.left() < m_bounds.left()) {
            moved.moveLeft(m_bounds.left());
        }
        if (moved.top() < m_bounds.top()) {
            moved.moveTop(m_bounds.top());
        }
        if (moved.right() > m_bounds.right()) {
            moved.moveRight(m_bounds.right());
        }
        if (moved.bottom() > m_bounds.bottom()) {
            moved.moveBottom(m_bounds.bottom());
        }
    }
    m_rect = moved;
}

SelectionModel::Handle SelectionModel::hitTest(const QPoint& physicalPos, int handleSizePhysical) const
{
    if (!m_active) {
        return Handle::None;
    }

    const QRect r = m_rect.normalized();
    if (r.width() <= 0 || r.height() <= 0) {
        return Handle::None;
    }

    const int half = std::max(2, handleSizePhysical / 2);
    const QPoint corners[8] = {
        r.topLeft(),
        QPoint(r.center().x(), r.top()),
        r.topRight(),
        QPoint(r.right(), r.center().y()),
        r.bottomRight(),
        QPoint(r.center().x(), r.bottom()),
        r.bottomLeft(),
        QPoint(r.left(), r.center().y()),
    };
    const Handle handles[8] = {Handle::TopLeft,
                               Handle::Top,
                               Handle::TopRight,
                               Handle::Right,
                               Handle::BottomRight,
                               Handle::Bottom,
                               Handle::BottomLeft,
                               Handle::Left};

    for (int i = 0; i < 8; ++i) {
        const QRect handleRect(corners[i].x() - half, corners[i].y() - half, half * 2, half * 2);
        if (handleRect.contains(physicalPos)) {
            return handles[i];
        }
    }

    // 边界附近（宽度内缩）也允许缩放，提升手感。
    const QRect outer = r.adjusted(-half, -half, half, half);
    const QRect inner = r.adjusted(half, half, -half, -half);
    if (outer.contains(physicalPos) && !inner.contains(physicalPos)) {
        if (physicalPos.y() <= r.top() + half) {
            return physicalPos.x() <= r.left() + half ? Handle::TopLeft : Handle::Top;
        }
        if (physicalPos.y() >= r.bottom() - half) {
            return physicalPos.x() <= r.left() + half ? Handle::BottomLeft : Handle::Bottom;
        }
        return physicalPos.x() <= r.left() + half ? Handle::Left : Handle::Right;
    }

    if (r.contains(physicalPos)) {
        return Handle::Inside;
    }
    return Handle::None;
}

void SelectionModel::setRect(const QRect& physicalRect)
{
    m_rect = clamped(physicalRect.normalized());
    m_active = !m_rect.isEmpty();
}

bool SelectionModel::hasUsableSelection() const
{
    const QRect r = rect();
    return r.width() >= 2 && r.height() >= 2;
}

}  // namespace snapocr::overlay
