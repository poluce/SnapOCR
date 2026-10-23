#pragma once

#include <QMainWindow>
#include <QString>

class QLabel;
class QPushButton;

namespace snapocr::ui {

// 主窗口：功能入口面板（截图/取字/设置），关闭时隐藏到托盘。
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void setHotkeyTexts(const QString& captureText, const QString& ocrText);
    void setStatusText(const QString& text);

signals:
    void captureRequested();
    void ocrRequested();
    void settingsRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QPushButton* m_captureButton = nullptr;
    QPushButton* m_ocrButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QLabel* m_hotkeyLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
};

}  // namespace snapocr::ui
