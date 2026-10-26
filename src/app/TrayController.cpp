#include "app/TrayController.h"

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

#include "snapocr/Version.h"
#include "ui/AppIcon.h"

namespace snapocr::app {

TrayController::TrayController(QObject* parent)
    : QObject(parent)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    m_menu = new QMenu;
    m_captureAction = m_menu->addAction(tr("截图"));
    m_ocrAction = m_menu->addAction(tr("屏幕取字"));
    QAction* historyAction = m_menu->addAction(tr("截图历史"));
    QAction* pinLatestAction = m_menu->addAction(tr("贴图最近一张"));
    m_menu->addSeparator();
    QAction* showAction = m_menu->addAction(tr("打开主窗口"));
    QAction* settingsAction = m_menu->addAction(tr("设置"));
    QAction* updateAction = m_menu->addAction(tr("检查更新"));
    m_menu->addSeparator();
    QAction* quitAction = m_menu->addAction(tr("退出"));

    connect(m_captureAction, &QAction::triggered, this, &TrayController::captureRequested);
    connect(m_ocrAction, &QAction::triggered, this, &TrayController::ocrRequested);
    connect(historyAction, &QAction::triggered, this, &TrayController::historyRequested);
    connect(pinLatestAction, &QAction::triggered, this, &TrayController::pinLatestRequested);
    connect(showAction, &QAction::triggered, this, &TrayController::showMainRequested);
    connect(settingsAction, &QAction::triggered, this, &TrayController::settingsRequested);
    connect(updateAction, &QAction::triggered, this, &TrayController::updateCheckRequested);
    connect(quitAction, &QAction::triggered, this, &TrayController::quitRequested);

    setHotkeyTexts(QStringLiteral("F1"), QStringLiteral("F4"));

    m_tray = new QSystemTrayIcon(ui::trayIcon(), this);
    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip(QStringLiteral("%1 %2")
                           .arg(QString::fromUtf8(snapocr::version::kAppName),
                                QString::fromUtf8(snapocr::version::kVersion)));

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    emit captureRequested();
                } else if (reason == QSystemTrayIcon::DoubleClick) {
                    emit showMainRequested();
                }
            });
}

TrayController::~TrayController()
{
    if (m_tray != nullptr) {
        m_tray->hide();
    }
    delete m_menu;
    m_menu = nullptr;
}

bool TrayController::isAvailable() const
{
    return m_tray != nullptr;
}

void TrayController::show()
{
    if (m_tray != nullptr) {
        m_tray->show();
    }
}

void TrayController::hide()
{
    if (m_tray != nullptr) {
        m_tray->hide();
    }
}

void TrayController::showMessage(const QString& title, const QString& body, bool warning)
{
    if (m_tray == nullptr) {
        return;
    }
    m_tray->showMessage(title, body, warning ? QSystemTrayIcon::Warning
                                             : QSystemTrayIcon::Information, 3000);
}

void TrayController::setHotkeyTexts(const QString& captureText, const QString& ocrText)
{
    if (m_captureAction != nullptr) {
        m_captureAction->setText(
            tr("截图\t%1").arg(captureText.isEmpty() ? QStringLiteral("F1") : captureText));
    }
    if (m_ocrAction != nullptr) {
        m_ocrAction->setText(
            tr("屏幕取字\t%1").arg(ocrText.isEmpty() ? QStringLiteral("F4") : ocrText));
    }
}

}  // namespace snapocr::app
