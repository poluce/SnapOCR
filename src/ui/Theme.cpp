#include "ui/Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace snapocr::ui {
namespace {

void applyLightTheme(QPalette& palette)
{
    palette.setColor(QPalette::Window, QColor(0xF7, 0xF7, 0xF8));
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

void applyDarkTheme(QPalette& palette)
{
    palette.setColor(QPalette::Window, QColor(0x1E, 0x1F, 0x22));
    palette.setColor(QPalette::WindowText, QColor(0xE6, 0xE8, 0xEB));
    palette.setColor(QPalette::Base, QColor(0x16, 0x17, 0x19));
    palette.setColor(QPalette::AlternateBase, QColor(0x24, 0x26, 0x29));
    palette.setColor(QPalette::Text, QColor(0xE6, 0xE8, 0xEB));
    palette.setColor(QPalette::Button, QColor(0x2A, 0x2C, 0x30));
    palette.setColor(QPalette::ButtonText, QColor(0xE6, 0xE8, 0xEB));
    palette.setColor(QPalette::Highlight, QColor(0x1F, 0x6F, 0xEB));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, QColor(0x2B, 0x2B, 0x2B));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(0x8A, 0x8F, 0x96));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x77, 0x7A, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x77, 0x7A, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x77, 0x7A, 0x80));
}

}  // namespace

void applyTheme(const QString& mode)
{
    QApplication* app = qobject_cast<QApplication*>(QApplication::instance());
    if (app == nullptr) {
        return;
    }

    bool dark = false;
    if (mode == QLatin1String("dark")) {
        dark = true;
    } else if (mode == QLatin1String("light")) {
        dark = false;
    } else {
        // 跟随系统：根据窗口背景色亮度判断（Windows 上由 Qt 反映系统主题）。
        const QColor windowColor = app->palette().color(QPalette::Window);
        dark = windowColor.lightness() < 128;
    }

    QPalette palette = app->style()->standardPalette();
    if (dark) {
        applyDarkTheme(palette);
    } else {
        applyLightTheme(palette);
    }
    app->setPalette(palette);

    app->setStyleSheet(dark ? QStringLiteral(
                                 "QToolTip { color: #E6E8EB; background-color: #2B2B2B;"
                                 " border: 1px solid #444; padding: 4px; }")
                            : QString());
}

}  // namespace snapocr::ui
