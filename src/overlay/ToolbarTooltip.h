#pragma once

#include <QString>
#include <QWidget>

namespace snapocr::overlay {

// 工具条自绘提示气泡。
//
// 为什么不用 QToolTip：
//   截图覆盖层是每屏一个全屏置顶窗口，QToolTip 也属于置顶窗口，
//   两者在 Windows 上的 z 序由激活顺序决定 —— 覆盖层一旦被重新 raise，
//   系统工具提示就会被盖在暗化截图下面，看起来像“提示显示不正常”。
//   自绘气泡显式 raise()，并随工具条一起隐藏，行为可控。
class ToolbarTooltip final : public QWidget
{
    Q_OBJECT

public:
    static ToolbarTooltip* instance();

    void showFor(QWidget* anchor, const QString& text);
    void hideNow();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ToolbarTooltip();

    QString m_text;
};

}  // namespace snapocr::overlay
