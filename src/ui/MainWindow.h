#pragma once

#include <QMainWindow>
#include <QString>

#include "common/Settings.h"

class QLabel;
class QPushButton;
class QScrollArea;

namespace snapocr::ui {

class SettingsPanel;
class TitleBar;

// 主窗口：操作入口 + 设置面板合并为一个窗口（无边框 + 自建标题栏）。
// 设置改动通过 settingsChanged() 立即通知上层应用并持久化。
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void setHotkeyTexts(const QString& captureText, const QString& ocrText);
    void setStatusText(const QString& text);
    void setSettings(const snapocr::common::AppSettings& settings);
    // 滚动到设置区域（托盘菜单「设置」用）
    void revealSettings();

signals:
    void captureRequested();
    void ocrRequested();
    void historyRequested();
    void settingsChanged(const snapocr::common::AppSettings& settings);

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void buildUi();
    void applyStyleSheet();
    QPushButton* makeActionCard(const QString& iconAction, const QString& title,
                                QLabel** hotkeyBadge, QWidget* parent);

    QPushButton* m_captureButton = nullptr;
    QPushButton* m_historyButton = nullptr;
    QPushButton* m_ocrButton = nullptr;
    QLabel* m_captureHotkeyBadge = nullptr;
    QLabel* m_ocrHotkeyBadge = nullptr;
    QLabel* m_statusLabel = nullptr;
    QScrollArea* m_scroll = nullptr;
    SettingsPanel* m_settingsPanel = nullptr;
    TitleBar* m_titleBar = nullptr;
    QWidget* m_settingsSection = nullptr;
    QWidget* m_contentWidget = nullptr;
    bool m_heightAdjusted = false;
};

}  // namespace snapocr::ui
