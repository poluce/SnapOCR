#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

#include "capture/CaptureTypes.h"
#include "overlay/SelectionModel.h"

namespace snapocr::overlay {


// 单显示器覆盖层：显示冻结画面 + 暗化 + 选区交互。
// 每块屏一个窗口，保证混合 DPI 下不整体缩放；坐标统一走物理像素换算。
class OverlayWindow final : public QWidget
{
    Q_OBJECT

public:
    OverlayWindow(const capture::ScreenFrame& frame, const QVector<capture::MonitorInfo>* monitors,
                  SelectionModel* selection, QWidget* parent = nullptr);

    // 编辑态下不再响应鼠标（由编辑器接管），但仍显示选区与遮罩。
    void setSelectionInteractive(bool interactive);
    bool isSelectionInteractive() const { return m_interactive; }

    const capture::MonitorInfo& monitor() const { return m_frame.monitor; }

    // 重新绘制（选区变化时由控制器调用）。
    void refresh();

    // 放大镜：控制器轮询光标位置驱动（不依赖鼠标移动事件）。
    void showMagnifierAt(const QPoint& globalLogical);
    void hideMagnifier();

    // F-CAP-15：窗口识别（控制器轮询驱动），悬停高亮 + 单击选中窗口区域
    void updateWindowHover(const QPointF& globalLogical);
    void setDetectWindows(bool enabled) { m_detectWindows = enabled; }
    void clearWindowHover();
    // 直接把某个窗口矩形应用为当前选区（窗口识别点击时调用）
    void applyWindowSelection(const QRect& physicalRect);

signals:
    void selectionChanged();
    void selectionReleased();
    void selectionConfirmed();
    void cancelled();
    void saveRequested();
    void copyRequested();
    // 取色：用户在覆盖层里按 C 复制当前像素颜色
    void colorCopyRequested(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPoint physicalFromGlobal(const QPointF& globalLogical) const;
    QRectF selectionInLocal() const;
    void paintHud(QPainter& painter, const QRectF& selectionLocal);
    void paintMagnifier(QPainter& painter);

    capture::ScreenFrame m_frame;
    const QVector<capture::MonitorInfo>* m_monitors = nullptr;
    SelectionModel* m_selection = nullptr;
    bool m_interactive = true;
    bool m_dragging = false;
    SelectionModel::Handle m_activeHandle = SelectionModel::Handle::None;
    // 放大镜状态（位置/取样点/是否显示）
    bool m_magnifierVisible = false;
    QPoint m_magnifierCursorLogical;
    QPoint m_magnifierSamplePhysical;

    bool m_hoverWindowVisible = false;
    QRect m_hoverWindowPhysical;  // 虚拟桌面物理坐标
    bool m_detectWindows = true;
};

}  // namespace snapocr::overlay
