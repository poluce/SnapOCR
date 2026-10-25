#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace snapocr::ui {

// 滚动长截图期间的悬浮控制条（F-CAP-16）：
// 提示用户滚动页面，并提供「完成 / 取消」。保持置顶但不抢焦点中的页面输入。
class ScrollingBar final : public QWidget
{
    Q_OBJECT

public:
    explicit ScrollingBar(QWidget* parent = nullptr);

    void setProgress(int frames, int stitchedHeight);

signals:
    void finishRequested();
    void cancelRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void applyStyleSheet();
    QLabel* m_hint = nullptr;
    QLabel* m_progress = nullptr;
};

}  // namespace snapocr::ui
