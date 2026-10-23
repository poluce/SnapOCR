#pragma once

#include <QRect>
#include <QRectF>

namespace snapocr::overlay {

// 选区模型：内部一律使用**物理像素**（虚拟桌面坐标），保证裁剪与导出一致。
// 拖拽/缩放以“操作起点快照 + 当前位移”计算，避免逐次增量导致的漂移。
class SelectionModel
{
public:
    enum class Handle
    {
        None,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
        Inside
    };

    enum class DragMode
    {
        None,
        Creating,
        Moving,
        Resizing
    };

    SelectionModel() = default;

    void setBounds(const QRect& physicalBounds);
    QRect bounds() const { return m_bounds; }

    bool isActive() const { return m_active; }
    DragMode dragMode() const { return m_mode; }

    // 规范化后的选区（宽高 >= 1）；未激活时返回空矩形。
    QRect rect() const;

    void reset();

    // 创建：从起点拖到当前点。
    void beginCreate(const QPoint& physicalPos);
    void updateCreate(const QPoint& physicalPos);

    // 移动与缩放：需要先命中 handle。
    void beginMove(const QPoint& physicalPos);
    void beginResize(Handle handle, const QPoint& physicalPos);
    void updateDrag(const QPoint& physicalPos);

    // 方向键微调（物理像素）；整块平移。
    void nudge(int dx, int dy);

    // 命中测试：handlesPhysical 为控制点边长（物理像素）。
    Handle hitTest(const QPoint& physicalPos, int handleSizePhysical) const;

    // 直接设置选区（用于全屏选择等），会夹取到 bounds。
    void setRect(const QRect& physicalRect);

    // 选区是否有效（宽高均 >= 2）。
    bool hasUsableSelection() const;

private:
    QRect clamped(const QRect& physicalRect) const;
    void applyDrag(const QPoint& physicalPos);

    QRect m_bounds;
    QRect m_rect;
    QRect m_dragStartRect;
    QPoint m_dragStartPos;
    Handle m_dragHandle = Handle::None;
    DragMode m_mode = DragMode::None;
    bool m_active = false;
};

}  // namespace snapocr::overlay
