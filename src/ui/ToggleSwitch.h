#pragma once

#include <QAbstractButton>

class QPropertyAnimation;

namespace snapocr::ui {

// 现代风格的开关控件（替代 QCheckBox）。
// 自绘圆角轨道 + 白色滑块，带 120ms 过渡动画；点击整块区域切换。
class ToggleSwitch final : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal knobPosition READ knobPosition WRITE setKnobPosition)

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

    qreal knobPosition() const { return m_knobPosition; }
    void setKnobPosition(qreal position);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void nextCheckState() override;

private:
    void animateTo(qreal target);

    qreal m_knobPosition = 0.0;  // 0 = 关，1 = 开
    bool m_hovered = false;
    QPropertyAnimation* m_animation = nullptr;
};

}  // namespace snapocr::ui
