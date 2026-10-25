#pragma once

#include <QWidget>

class QLabel;
class QToolButton;

namespace snapocr::ui {

// 自建标题栏：无边框窗口自绘标题、拖动、关闭按钮。
// 用 windowHandle()->startSystemMove() 交给系统拖动，避免手写 move 跟随带来的抖动。
class TitleBar final : public QWidget
{
    Q_OBJECT

public:
    // showMinimize / showClose：按窗口类型显示对应的系统按钮
    explicit TitleBar(const QString& title, QWidget* parent = nullptr,
                      bool showMinimize = false, bool showClose = true);

    void setTitle(const QString& title);

signals:
    void closeRequested();
    void minimizeRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* m_titleLabel = nullptr;
    QToolButton* m_closeButton = nullptr;
    QToolButton* m_minimizeButton = nullptr;
};

}  // namespace snapocr::ui
