#include "ui/Theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>
#include <QWidget>

namespace snapocr::ui {
namespace {

bool g_dark = false;
QString g_requestedMode = QStringLiteral("system");
bool g_systemHookInstalled = false;

ThemeColors lightColors()
{
    ThemeColors colors;
    colors.window = QColor(0xF7, 0xF8, 0xFA);
    colors.surface = QColor(0xFF, 0xFF, 0xFF);
    colors.surfaceHover = QColor(0xFB, 0xFC, 0xFF);
    colors.border = QColor(0xE5, 0xE8, 0xED);
    colors.borderStrong = QColor(0xC7, 0xD6, 0xF5);
    colors.text = QColor(0x1F, 0x23, 0x28);
    colors.textMuted = QColor(0x52, 0x58, 0x62);
    colors.textFaint = QColor(0x9A, 0xA0, 0xA8);
    colors.accent = QColor(0x1F, 0x6F, 0xEB);
    colors.accentSoft = QColor(0xE8, 0xF0, 0xFE);
    colors.danger = QColor(0xE8, 0x46, 0x4A);
    colors.scrollbar = QColor(0xD2, 0xD7, 0xDF);
    return colors;
}

ThemeColors darkColors()
{
    ThemeColors colors;
    colors.window = QColor(0x1E, 0x1F, 0x22);
    colors.surface = QColor(0x28, 0x2A, 0x2E);
    colors.surfaceHover = QColor(0x31, 0x34, 0x39);
    colors.border = QColor(0x3A, 0x3D, 0x43);
    colors.borderStrong = QColor(0x4C, 0x63, 0x8F);
    colors.text = QColor(0xE6, 0xE8, 0xEB);
    colors.textMuted = QColor(0xB4, 0xB9, 0xC0);
    colors.textFaint = QColor(0x8A, 0x8F, 0x96);
    colors.accent = QColor(0x4C, 0x8D, 0xFF);
    colors.accentSoft = QColor(0x26, 0x35, 0x4D);
    colors.danger = QColor(0xD9, 0x4A, 0x4E);
    colors.scrollbar = QColor(0x45, 0x49, 0x50);
    return colors;
}

void applyLightPalette(QPalette& palette)
{
    palette.setColor(QPalette::Window, QColor(0xF7, 0xF8, 0xFA));
    palette.setColor(QPalette::WindowText, QColor(0x1F, 0x23, 0x28));
    palette.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
    palette.setColor(QPalette::AlternateBase, QColor(0xF0, 0xF0, 0xF2));
    palette.setColor(QPalette::Text, QColor(0x1F, 0x23, 0x28));
    palette.setColor(QPalette::Button, QColor(0xEC, 0xEC, 0xEF));
    palette.setColor(QPalette::ButtonText, QColor(0x1F, 0x23, 0x28));
    palette.setColor(QPalette::Highlight, QColor(0x1F, 0x6F, 0xEB));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, QColor(0x2B, 0x2B, 0x2B));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(0x88, 0x8D, 0x94));
}

void applyDarkPalette(QPalette& palette)
{
    palette.setColor(QPalette::Window, QColor(0x1E, 0x1F, 0x22));
    palette.setColor(QPalette::WindowText, QColor(0xE6, 0xE8, 0xEB));
    palette.setColor(QPalette::Base, QColor(0x16, 0x17, 0x19));
    palette.setColor(QPalette::AlternateBase, QColor(0x24, 0x26, 0x29));
    palette.setColor(QPalette::Text, QColor(0xE6, 0xE8, 0xEB));
    palette.setColor(QPalette::Button, QColor(0x2A, 0x2C, 0x30));
    palette.setColor(QPalette::ButtonText, QColor(0xE6, 0xE8, 0xEB));
    palette.setColor(QPalette::Highlight, QColor(0x4C, 0x8D, 0xFF));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, QColor(0x2B, 0x2B, 0x2B));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(0x8A, 0x8F, 0x96));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x77, 0x7A, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x77, 0x7A, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x77, 0x7A, 0x80));
}

bool systemPrefersDark()
{
    // Qt 6.5+ 提供系统配色方案；拿不到时退化为“浅色”。
    QGuiApplication* app = qobject_cast<QGuiApplication*>(QGuiApplication::instance());
    if (app == nullptr || app->styleHints() == nullptr) {
        return false;
    }
    return app->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

void applyCurrentTheme()
{
    QApplication* app = qobject_cast<QApplication*>(QApplication::instance());
    if (app == nullptr) {
        return;
    }

    g_dark = g_requestedMode == QLatin1String("dark")
             || (g_requestedMode != QLatin1String("light") && systemPrefersDark());

    QPalette palette = app->style()->standardPalette();
    if (g_dark) {
        applyDarkPalette(palette);
    } else {
        applyLightPalette(palette);
    }
    app->setPalette(palette);

    // 全局兜底：未单独应用样式的控件也跟随主题
    app->setStyleSheet(styled(QStringLiteral(R"(
        QToolTip { color: {text}; background-color: {surface};
                   border: 1px solid {border}; padding: 4px; }
        QMenu { background: {surface}; border: 1px solid {border}; border-radius: 8px; padding: 4px; }
        QMenu::item { color: {text}; padding: 6px 18px; border-radius: 6px; }
        QMenu::item:selected { background: {accentSoft}; color: {accent}; }
        QMenu::separator { height: 1px; background: {border}; margin: 4px 8px; }
    )")));

    ThemeNotifier::instance().notify();

    // 自绘控件（标题栏/开关/图标）不会因为 QSS 变化自动重绘
    if (QApplication* guiApp = qobject_cast<QApplication*>(QApplication::instance())) {
        const QWidgetList widgets = guiApp->allWidgets();
        for (QWidget* widget : widgets) {
            if (widget != nullptr) {
                widget->update();
            }
        }
    }
}

}  // namespace

ThemeNotifier& ThemeNotifier::instance()
{
    static ThemeNotifier notifier;
    return notifier;
}

void ThemeNotifier::notify()
{
    emit themeChanged();
}

bool isDarkTheme()
{
    return g_dark;
}

ThemeColors themeColors()
{
    return g_dark ? darkColors() : lightColors();
}

QString styled(const QString& templateQss)
{
    const ThemeColors colors = themeColors();
    QString result = templateQss;
    const struct
    {
        const char* token;
        QColor color;
    } tokens[] = {
        {"{window}", colors.window},       {"{surface}", colors.surface},
        {"{surfaceHover}", colors.surfaceHover}, {"{border}", colors.border},
        {"{borderStrong}", colors.borderStrong}, {"{text}", colors.text},
        {"{textMuted}", colors.textMuted}, {"{textFaint}", colors.textFaint},
        {"{accent}", colors.accent},       {"{accentSoft}", colors.accentSoft},
        {"{danger}", colors.danger},       {"{scrollbar}", colors.scrollbar},
    };
    for (const auto& token : tokens) {
        result.replace(QString::fromLatin1(token.token),
                       token.color.name(QColor::HexRgb));
    }
    return result;
}

void applyTheme(const QString& mode)
{
    g_requestedMode = mode.isEmpty() ? QStringLiteral("system") : mode;

    // 系统主题变化时自动跟随（只安装一次）
    if (!g_systemHookInstalled) {
        if (QGuiApplication* app = qobject_cast<QGuiApplication*>(QGuiApplication::instance())) {
            if (app->styleHints() != nullptr) {
                QObject::connect(app->styleHints(), &QStyleHints::colorSchemeChanged, app, []() {
                    if (g_requestedMode == QLatin1String("system")) {
                        applyCurrentTheme();
                    }
                });
                g_systemHookInstalled = true;
            }
        }
    }

    applyCurrentTheme();
}

}  // namespace snapocr::ui
