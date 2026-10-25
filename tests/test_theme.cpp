#include <QtTest>

#include <QApplication>
#include <QImage>
#include <QPalette>

#include "ui/MainWindow.h"
#include "ui/Theme.h"

using namespace snapocr::ui;

// 主题一致性（阶段三）：light/dark 两套色板、QSS token 替换、主窗口在两种主题下都能渲染。
class ThemeTest final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void lightAndDarkTokensDiffer();
    void styledReplacesTokens();
    void paletteFollowsTheme();
    void mainWindowRendersInBothThemes();
};

void ThemeTest::cleanup()
{
    applyTheme(QStringLiteral("light"));
}

void ThemeTest::lightAndDarkTokensDiffer()
{
    applyTheme(QStringLiteral("light"));
    QVERIFY(!isDarkTheme());
    const ThemeColors light = themeColors();

    applyTheme(QStringLiteral("dark"));
    QVERIFY(isDarkTheme());
    const ThemeColors dark = themeColors();

    QVERIFY(light.window != dark.window);
    QVERIFY(light.surface != dark.surface);
    QVERIFY(light.text != dark.text);
    QCOMPARE(dark.window, QColor(0x1E, 0x1F, 0x22));
    QCOMPARE(light.window, QColor(0xF7, 0xF8, 0xFA));
}

void ThemeTest::styledReplacesTokens()
{
    applyTheme(QStringLiteral("dark"));
    const ThemeColors colors = themeColors();
    const QString qss = styled(QStringLiteral("QWidget { background: {window}; color: {text}; }"));
    QVERIFY(qss.contains(colors.window.name(QColor::HexRgb)));
    QVERIFY(qss.contains(colors.text.name(QColor::HexRgb)));
    QVERIFY(!qss.contains(QStringLiteral("{window}")));
}

void ThemeTest::paletteFollowsTheme()
{
    applyTheme(QStringLiteral("dark"));
    QCOMPARE(QApplication::palette().color(QPalette::Window), QColor(0x1E, 0x1F, 0x22));

    applyTheme(QStringLiteral("light"));
    QCOMPARE(QApplication::palette().color(QPalette::Window), QColor(0xF7, 0xF8, 0xFA));
}

void ThemeTest::mainWindowRendersInBothThemes()
{
    applyTheme(QStringLiteral("light"));
    MainWindow window;
    window.resize(520, 460);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(40);
    const QImage lightShot = window.grab().toImage();
    QVERIFY(!lightShot.isNull());

    applyTheme(QStringLiteral("dark"));
    QTest::qWait(40);
    const QImage darkShot = window.grab().toImage();
    QVERIFY(!darkShot.isNull());

    // 两套主题的渲染结果必须不同（否则说明样式没跟着切换）
    QVERIFY(lightShot != darkShot);

    const QString previewDir = qEnvironmentVariable("SNAPCR_THEME_PREVIEW_DIR");
    if (!previewDir.isEmpty()) {
        lightShot.save(previewDir + QStringLiteral("/phase3-theme-light.png"), "PNG");
        darkShot.save(previewDir + QStringLiteral("/phase3-theme-dark.png"), "PNG");
    }
}

QTEST_MAIN(ThemeTest)

#include "test_theme.moc"
