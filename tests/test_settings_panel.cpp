#include <QtTest>

#include <QSlider>

#include "common/Settings.h"
#include "ui/MainWindow.h"
#include "ui/SettingsPanel.h"

using namespace snapocr;

// 设置面板（合并进主窗口后）的回归：
// 1) setSettings -> settings() 必须原样往返；
// 2) 用户改动必须发出 settingsChanged（即时生效的基础）；
// 3) 加载配置时不能误发变更（否则上层会重复应用）。
class SettingsPanelTest final : public QObject
{
    Q_OBJECT

private slots:
    void settingsRoundTrip();
    void sliderChangeEmitsSettingsChanged();
    void loadingDoesNotEmit();
    void mainWindowEmbedsSettingsPanel();
};

void SettingsPanelTest::settingsRoundTrip()
{
    common::AppSettings input;
    input.captureHotkey = QStringLiteral("Ctrl+Shift+A");
    input.ocrHotkey = QStringLiteral("F9");
    input.saveDirectory = QStringLiteral("D:/shots");
    input.saveFormat = QStringLiteral("jpg");
    input.jpgQuality = 77;
    input.nameTemplate = QStringLiteral("shot_yyyyMMdd");
    input.theme = QStringLiteral("dark");
    input.startWithWindows = true;
    input.ocrAutoCopy = false;
    input.logLevel = QStringLiteral("debug");
    input.delaySeconds = 3;
    input.includeCursor = true;

    ui::SettingsPanel panel;
    panel.setSettings(input);
    const common::AppSettings output = panel.settings();

    QCOMPARE(output.captureHotkey, input.captureHotkey);
    QCOMPARE(output.ocrHotkey, input.ocrHotkey);
    QCOMPARE(output.saveDirectory, input.saveDirectory);
    QCOMPARE(output.saveFormat, input.saveFormat);
    QCOMPARE(output.jpgQuality, input.jpgQuality);
    QCOMPARE(output.nameTemplate, input.nameTemplate);
    QCOMPARE(output.theme, input.theme);
    QCOMPARE(output.startWithWindows, input.startWithWindows);
    QCOMPARE(output.ocrAutoCopy, input.ocrAutoCopy);
    QCOMPARE(output.logLevel, input.logLevel);
    QCOMPARE(output.delaySeconds, input.delaySeconds);
    QCOMPARE(output.includeCursor, input.includeCursor);
}

void SettingsPanelTest::sliderChangeEmitsSettingsChanged()
{
    ui::SettingsPanel panel;
    common::AppSettings input;
    input.jpgQuality = 60;
    input.saveFormat = QStringLiteral("jpg");
    panel.setSettings(input);

    QSignalSpy spy(&panel, &ui::SettingsPanel::settingsChanged);
    auto* slider = panel.findChild<QSlider*>();
    QVERIFY(slider != nullptr);
    slider->setValue(88);

    QVERIFY(spy.count() >= 1);
    QCOMPARE(panel.settings().jpgQuality, 88);
}

void SettingsPanelTest::loadingDoesNotEmit()
{
    ui::SettingsPanel panel;
    QSignalSpy spy(&panel, &ui::SettingsPanel::settingsChanged);
    common::AppSettings input;
    input.theme = QStringLiteral("dark");
    input.jpgQuality = 70;
    panel.setSettings(input);
    QCOMPARE(spy.count(), 0);
}

void SettingsPanelTest::mainWindowEmbedsSettingsPanel()
{
    ui::MainWindow window;
    QVERIFY(window.findChild<ui::SettingsPanel*>() != nullptr);
    window.setSettings(common::AppSettings{});
    window.setHotkeyTexts(QStringLiteral("Ctrl+F1"), QStringLiteral("Ctrl+F4"));
    window.setStatusText(QStringLiteral("就绪"));

    // 仅在设置环境变量时输出预览图，供人工核对外观（不参与断言）。
    if (!qEnvironmentVariableIsEmpty("SNAPCR_MAIN_PREVIEW")) {
        window.resize(672, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);
        const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/main_window_preview.png");
        QVERIFY(window.grab().save(path));
    }

    window.revealSettings();  // 滚动到设置区（仅验证接口不崩溃）
}

QTEST_MAIN(SettingsPanelTest)

#include "test_settings_panel.moc"
