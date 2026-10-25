#pragma once

#include <QColor>
#include <QObject>
#include <QString>

namespace snapocr::ui {

// 主题模式：light / dark / system（跟随系统）。
// 覆盖层（深色半透明）不受影响；主窗口、设置、对话框、悬浮控制条都走主题色板。
void applyTheme(const QString& mode);

// 当前是否暗色主题（applyTheme 之后有效）。
bool isDarkTheme();

// 主题色板：所有自绘窗口的 QSS 只允许使用这里的颜色。
struct ThemeColors
{
    QColor window;        // 窗口背景
    QColor surface;       // 卡片/输入框背景
    QColor surfaceHover;  // 悬停背景
    QColor border;        // 常规边框
    QColor borderStrong;  // 悬停/聚焦边框
    QColor text;          // 主文本
    QColor textMuted;     // 次级文本
    QColor textFaint;     // 占位/说明文本
    QColor accent;        // 强调色
    QColor accentSoft;    // 强调色浅底（选中态）
    QColor danger;        // 危险/关闭悬停
    QColor scrollbar;     // 滚动条滑块
};

ThemeColors themeColors();

// 把 QSS 模板里的 {window} {surface} {border} {text} {accent} 等占位符
// 替换成当前主题颜色（返回可直接 setStyleSheet 的字符串）。
QString styled(const QString& templateQss);

// 主题变更通知：已经创建的窗口连接它来重新应用样式。
class ThemeNotifier final : public QObject
{
    Q_OBJECT

public:
    static ThemeNotifier& instance();
    // 由 applyTheme 调用：广播主题变更
    void notify();

signals:
    void themeChanged();
};

}  // namespace snapocr::ui
