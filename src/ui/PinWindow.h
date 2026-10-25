#pragma once

#include <QImage>
#include <QPoint>
#include <QWidget>

class QLabel;

namespace snapocr::ui {

// 贴图窗口（F-ANN-19）：把截图钉在屏幕上，置顶显示。
// 交互：
//   - 左键拖动移动
//   - 滚轮缩放（0.1x–4x，以光标为锚点）
//   - Ctrl+滚轮调透明度
//   - R 旋转 90°
//   - Esc / 右键菜单关闭
//   - Ctrl+C 复制到剪贴板、Ctrl+S 保存
class PinWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit PinWindow(const QImage& image, QWidget* parent = nullptr);

    QImage image() const { return m_image; }
    qreal zoom() const { return m_zoom; }
    void setZoom(qreal zoom);

signals:
    void copyRequested(const QImage& image);
    void saveRequested(const QImage& image);
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void applyZoom(qreal zoom, const QPoint& anchorLocal);
    void rotateBy90();
    void updateWindowSize();

    QImage m_image;
    qreal m_zoom = 1.0;
    int m_rotation = 0;  // 0/90/180/270
    bool m_dragging = false;
    QPoint m_dragOffset;
    int m_opacityPercent = 100;
    qreal m_devicePixelRatio = 1.0;
    QPoint m_zoomAnchor;
};

}  // namespace snapocr::ui
