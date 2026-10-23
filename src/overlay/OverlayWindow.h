#pragma once

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

signals:
    void selectionChanged();
    void selectionReleased();
    void selectionConfirmed();
    void cancelled();
    void saveRequested();
    void copyRequested();

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

    capture::ScreenFrame m_frame;
    const QVector<capture::MonitorInfo>* m_monitors = nullptr;
    SelectionModel* m_selection = nullptr;
    bool m_interactive = true;
    bool m_dragging = false;
    SelectionModel::Handle m_activeHandle = SelectionModel::Handle::None;
};

}  // namespace snapocr::overlay
