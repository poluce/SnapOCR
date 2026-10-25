#include <QtTest>

#include <QPushButton>
#include <QSlider>

#include "common/Settings.h"
#include "ui/SettingsDialog.h"

using namespace snapocr;

// 设置对话框重做后的数据绑定回归：构造 -> settings() 必须原样返回传入值。
class SettingsDialogTest final : public QObject
{
    Q_OBJECT

private slots:
    void settingsRoundTrip();
    void themeDefaultsToSystem();
    void hasPrimaryAndSecondaryButtons();
};

void SettingsDialogTest::settingsRoundTrip()
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

    ui::SettingsDialog dialog(input);
    const common::AppSettings output = dialog.settings();

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
}

void SettingsDialogTest::themeDefaultsToSystem()
{
    common::AppSettings input;
    input.theme = QStringLiteral("system");
    ui::SettingsDialog dialog(input);
    QCOMPARE(dialog.settings().theme, QStringLiteral("system"));
}

void SettingsDialogTest::hasPrimaryAndSecondaryButtons()
{
    ui::SettingsDialog dialog{common::AppSettings{}};
    auto* primary = dialog.findChild<QPushButton*>(QStringLiteral("primaryButton"));
    auto* secondary = dialog.findChild<QPushButton*>(QStringLiteral("secondaryButton"));
    QVERIFY(primary != nullptr);
    QVERIFY(secondary != nullptr);
    QCOMPARE(primary->text(), QStringLiteral("确定"));
    QVERIFY(dialog.findChild<QSlider*>() != nullptr);  // JPG 质量滑块
}

QTEST_MAIN(SettingsDialogTest)

#include "test_settings_dialog.moc"
