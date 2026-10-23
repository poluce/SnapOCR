#pragma once

#include <QDialog>

#include "common/Settings.h"

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QSpinBox;

namespace snapocr::ui {

// 设置对话框：读取 AppSettings，确认后通过 settings() 返回新值。
class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const snapocr::common::AppSettings& settings, QWidget* parent = nullptr);

    snapocr::common::AppSettings settings() const;

private:
    void loadIntoUi(const snapocr::common::AppSettings& settings);
    void collectFromUi();

    snapocr::common::AppSettings m_settings;

    QKeySequenceEdit* m_captureHotkey = nullptr;
    QKeySequenceEdit* m_ocrHotkey = nullptr;
    QLineEdit* m_saveDirectory = nullptr;
    QComboBox* m_format = nullptr;
    QSpinBox* m_jpgQuality = nullptr;
    QLineEdit* m_nameTemplate = nullptr;
    QComboBox* m_theme = nullptr;
    QCheckBox* m_autoStart = nullptr;
    QCheckBox* m_ocrAutoCopy = nullptr;
    QComboBox* m_logLevel = nullptr;
};

}  // namespace snapocr::ui
