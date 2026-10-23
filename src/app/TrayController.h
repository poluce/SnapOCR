#pragma once

#include <QObject>
#include <QString>

class QSystemTrayIcon;
class QMenu;
class QAction;

namespace snapocr::app {

// 托盘常驻：菜单触发截图/取字/设置/退出，并提供气泡提示。
class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QObject* parent = nullptr);
    ~TrayController() override;

    bool isAvailable() const;
    void show();
    void hide();

    void showMessage(const QString& title, const QString& body, bool warning = false);

    void setHotkeyTexts(const QString& captureText, const QString& ocrText);

signals:
    void captureRequested();
    void ocrRequested();
    void settingsRequested();
    void showMainRequested();
    void quitRequested();

private:
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_menu = nullptr;
    QAction* m_captureAction = nullptr;
    QAction* m_ocrAction = nullptr;
};

}  // namespace snapocr::app
