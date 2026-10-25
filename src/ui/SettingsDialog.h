#pragma once

#include <QDialog>
#include <QVector>

#include "common/Settings.h"

class QButtonGroup;
class QComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QSlider;
class QLabel;
class QVBoxLayout;

namespace snapocr::ui {

class TitleBar;
class ToggleSwitch;

// 设置对话框：读取 AppSettings，确认后通过 settings() 返回新值。
// 自建无边框窗口 + 自绘标题栏，卡片式分组布局。
class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const snapocr::common::AppSettings& settings, QWidget* parent = nullptr);

    snapocr::common::AppSettings settings() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void buildUi();
    void applyStyleSheet();
    void loadIntoUi(const snapocr::common::AppSettings& settings);
    void collectFromUi();

    // 卡片分组：小标题 + 白色圆角卡片
    QWidget* makeSection(const QString& caption, const QVector<QWidget*>& rows);
    // 一行设置项：左侧名称 + 右侧控件
    QWidget* makeRow(const QString& label, QWidget* control);
    QWidget* makeToggleRow(const QString& label, ToggleSwitch* toggle);
    QWidget* makeSeparator();

    snapocr::common::AppSettings m_settings;

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
    QVBoxLayout* m_contentLayout = nullptr;
    QWidget* m_contentWidget = nullptr;
    QWidget* m_footerWidget = nullptr;
    TitleBar* m_titleBar = nullptr;
    bool m_heightAdjusted = false;
};

}  // namespace snapocr::ui
