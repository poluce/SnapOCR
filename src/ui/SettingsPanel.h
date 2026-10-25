#pragma once

#include <QVector>
#include <QWidget>

#include "common/Settings.h"

class QButtonGroup;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QSlider;
class QVBoxLayout;

namespace snapocr::ui {

class ToggleSwitch;

// 设置面板：卡片式分组 + 开关/分段选择/滑块。
// 与旧版 SettingsDialog 的差别：没有「确定/取消」，任何改动立即通过
// settingsChanged() 通知上层应用并持久化（现代小工具的即时生效模式）。
class SettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget* parent = nullptr);

    void setSettings(const snapocr::common::AppSettings& settings);
    snapocr::common::AppSettings settings() const;

signals:
    void settingsChanged(const snapocr::common::AppSettings& settings);

private:
    void buildUi();
    void applyStyleSheet();
    void emitChanged();

    QWidget* makeRow(const QString& label, QWidget* control);
    QWidget* makeToggleRow(const QString& label, ToggleSwitch* toggle);
    QWidget* makeSeparator();
    QWidget* makeGroupGap();

    QKeySequenceEdit* m_captureHotkey = nullptr;
    QKeySequenceEdit* m_ocrHotkey = nullptr;
    QLineEdit* m_saveDirectory = nullptr;
    QComboBox* m_format = nullptr;
    QSlider* m_jpgQuality = nullptr;
    QLabel* m_jpgQualityValue = nullptr;
    QLineEdit* m_nameTemplate = nullptr;
    QButtonGroup* m_themeGroup = nullptr;
    ToggleSwitch* m_autoStart = nullptr;
    ToggleSwitch* m_ocrAutoCopy = nullptr;
    QComboBox* m_logLevel = nullptr;
    QComboBox* m_delaySeconds = nullptr;
    ToggleSwitch* m_includeCursor = nullptr;
    ToggleSwitch* m_pinOnCopy = nullptr;
    bool m_loading = false;
};

}  // namespace snapocr::ui
