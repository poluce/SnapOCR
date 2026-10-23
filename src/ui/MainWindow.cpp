#include "ui/MainWindow.h"

#include <QCloseEvent>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "snapocr/Version.h"
#include "ui/AppIcon.h"

namespace snapocr::ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SnapOCR"));
    setWindowIcon(ui::appIcon());
    resize(560, 380);
    setMinimumSize(460, 330);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("SnapOCR"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* subtitle = new QLabel(QStringLiteral("Windows 截图 · 标注 · 文字识别"), central);
    subtitle->setEnabled(false);
    layout->addWidget(subtitle);

    auto* grid = new QGridLayout;
    grid->setSpacing(12);

    m_captureButton = new QPushButton(QStringLiteral("截图"), central);
    m_captureButton->setMinimumHeight(64);
    m_ocrButton = new QPushButton(QStringLiteral("屏幕取字"), central);
    m_ocrButton->setMinimumHeight(64);
    m_settingsButton = new QPushButton(QStringLiteral("设置"), central);
    m_settingsButton->setMinimumHeight(40);

    grid->addWidget(m_captureButton, 0, 0);
    grid->addWidget(m_ocrButton, 0, 1);
    grid->addWidget(m_settingsButton, 1, 0, 1, 2);
    layout->addLayout(grid);

    m_hotkeyLabel = new QLabel(central);
    m_hotkeyLabel->setWordWrap(true);
    layout->addWidget(m_hotkeyLabel);

    m_statusLabel = new QLabel(central);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setEnabled(false);
    layout->addWidget(m_statusLabel);

    layout->addStretch(1);

    auto* hint = new QLabel(
        QStringLiteral("截图后可在选区工具条上标注、复制、保存或提取文字。关闭窗口将隐藏到托盘，"
                       "托盘菜单可退出。"),
        central);
    hint->setWordWrap(true);
    hint->setEnabled(false);
    layout->addWidget(hint);

    setCentralWidget(central);

    connect(m_captureButton, &QPushButton::clicked, this, &MainWindow::captureRequested);
    connect(m_ocrButton, &QPushButton::clicked, this, &MainWindow::ocrRequested);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::settingsRequested);

    setHotkeyTexts(QStringLiteral("F1"), QStringLiteral("F4"));
    statusBar()->showMessage(
        QStringLiteral("版本 %1").arg(QString::fromUtf8(snapocr::version::kVersion)));
}

void MainWindow::setHotkeyTexts(const QString& captureText, const QString& ocrText)
{
    m_captureButton->setText(
        QStringLiteral("截图（%1）").arg(captureText.isEmpty() ? QStringLiteral("F1") : captureText));
    m_ocrButton->setText(QStringLiteral("屏幕取字（%1）")
                             .arg(ocrText.isEmpty() ? QStringLiteral("F4") : ocrText));
    m_hotkeyLabel->setText(QStringLiteral("全局热键：%1 截图 / %2 取字（可在设置中修改）")
                               .arg(captureText.isEmpty() ? QStringLiteral("F1") : captureText,
                                    ocrText.isEmpty() ? QStringLiteral("F4") : ocrText));
}

void MainWindow::setStatusText(const QString& text)
{
    m_statusLabel->setText(text);
    statusBar()->showMessage(text, 5000);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // 关闭时隐藏到托盘，由托盘菜单“退出”真正退出。
    event->ignore();
    hide();
}

}  // namespace snapocr::ui
