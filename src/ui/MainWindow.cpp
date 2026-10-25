#include "ui/Theme.h"
#include "ui/MainWindow.h"

#include <QCloseEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QShowEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include "snapocr/Version.h"
#include "ui/AppIcon.h"
#include "ui/SettingsPanel.h"
#include "ui/TitleBar.h"
#include "ui/ToolIcons.h"

namespace snapocr::ui {
namespace {

constexpr int kShadowMargin = 16;
constexpr int kWindowWidth = 520;

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SnapOCR"));
    setWindowIcon(ui::appIcon());
    // 无边框 + 自建标题栏，与设置窗口同一套视觉语言。
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);

    buildUi();
    applyStyleSheet();
    connect(&ui::ThemeNotifier::instance(), &ui::ThemeNotifier::themeChanged, this,
            &MainWindow::applyStyleSheet);
    setHotkeyTexts(QStringLiteral("F1"), QStringLiteral("F4"));
}

void MainWindow::buildUi()
{
    // QMainWindow 有自带的布局（central widget / dock / statusbar），
    // 不能直接给它设 QLayout —— 那样内容不会被渲染。这里用一个透明容器
    // 作为 central widget，再在里面做阴影边距。
    auto* wrapper = new QWidget(this);
    wrapper->setObjectName(QStringLiteral("windowWrapper"));
    wrapper->setAttribute(Qt::WA_TranslucentBackground, true);

    auto* outer = new QVBoxLayout(wrapper);
    outer->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);
    outer->setSpacing(0);

    auto* surface = new QWidget(wrapper);
    surface->setObjectName(QStringLiteral("windowSurface"));
    surface->setAttribute(Qt::WA_StyledBackground, true);
    outer->addWidget(surface);

    auto* shadow = new QGraphicsDropShadowEffect(surface);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(0, 0, 0, 60));
    surface->setGraphicsEffect(shadow);

    auto* surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(0, 0, 0, 0);
    surfaceLayout->setSpacing(0);

    m_titleBar = new TitleBar(QStringLiteral("SnapOCR"), surface, /*showMinimize=*/true,
                              /*showClose=*/true);
    surfaceLayout->addWidget(m_titleBar);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);

    // ---- 可滚动内容 ----
    m_scroll = new QScrollArea(surface);
    m_scroll->setObjectName(QStringLiteral("contentScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(m_scroll);
    content->setObjectName(QStringLiteral("contentArea"));
    m_contentWidget = content;
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 12, 18, 6);
    contentLayout->setSpacing(6);

    // ---- 操作入口（紧凑两行，不再放大标题/说明） ----
    m_captureButton = makeActionCard(QStringLiteral("camera"), QStringLiteral("截图"),
                                     &m_captureHotkeyBadge, content);
    m_ocrButton = makeActionCard(QStringLiteral("ocr"), QStringLiteral("屏幕取字"),
                                 &m_ocrHotkeyBadge, content);
    contentLayout->addWidget(m_captureButton);
    contentLayout->addWidget(m_ocrButton);

    m_historyButton = makeActionCard(QStringLiteral("history"), QStringLiteral("截图历史"),
                                     nullptr, content);
    contentLayout->addWidget(m_historyButton);

    connect(m_captureButton, &QPushButton::clicked, this, &MainWindow::captureRequested);
    connect(m_ocrButton, &QPushButton::clicked, this, &MainWindow::ocrRequested);
    connect(m_historyButton, &QPushButton::clicked, this, &MainWindow::historyRequested);

    // ---- 设置（同一窗口内） ----
    auto* settingsSection = new QWidget(content);
    m_settingsSection = settingsSection;
    auto* settingsLayout = new QVBoxLayout(settingsSection);
    settingsLayout->setContentsMargins(0, 10, 0, 0);
    settingsLayout->setSpacing(0);
    m_settingsPanel = new SettingsPanel(settingsSection);
    settingsLayout->addWidget(m_settingsPanel);
    contentLayout->addWidget(settingsSection);

    connect(m_settingsPanel, &SettingsPanel::settingsChanged, this, &MainWindow::settingsChanged);

    connect(m_settingsPanel, &SettingsPanel::settingsChanged, this, &MainWindow::settingsChanged);

    contentLayout->addStretch(1);

    // ---- 底部状态 ----
    auto* footer = new QWidget(content);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(8);
    m_statusLabel = new QLabel(QStringLiteral("就绪"), footer);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    footerLayout->addWidget(m_statusLabel, 1);

    auto* versionLabel =
        new QLabel(QStringLiteral("v%1").arg(QString::fromUtf8(snapocr::version::kVersion)), footer);
    versionLabel->setObjectName(QStringLiteral("versionLabel"));
    footerLayout->addWidget(versionLabel, 0, Qt::AlignRight);
    contentLayout->addWidget(footer);

    m_scroll->setWidget(content);
    surfaceLayout->addWidget(m_scroll, 1);

    setCentralWidget(wrapper);

    setFixedWidth(kWindowWidth + kShadowMargin * 2);
    setMinimumHeight(360);
    resize(kWindowWidth + kShadowMargin * 2, 640);
}

QPushButton* MainWindow::makeActionCard(const QString& iconAction, const QString& title,
                                        QLabel** hotkeyBadge, QWidget* parent)
{
    auto* card = new QPushButton(parent);
    card->setObjectName(QStringLiteral("actionCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setFixedHeight(46);

    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 0, 14, 0);
    layout->setSpacing(12);

    auto* iconLabel = new QLabel(card);
    iconLabel->setPixmap(
        ui::actionIcon(iconAction, QColor(0x1F, 0x6F, 0xEB)).pixmap(QSize(18, 18), 2.0));
    iconLabel->setFixedSize(18, 18);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(iconLabel, 0, Qt::AlignVCenter);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("actionTitle"));
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(titleLabel, 1, Qt::AlignVCenter);

    auto* badge = new QLabel(card);
    badge->setObjectName(QStringLiteral("hotkeyBadge"));
    badge->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(badge, 0, Qt::AlignVCenter);
    if (hotkeyBadge != nullptr) {
        *hotkeyBadge = badge;
    }
    return card;
}

void MainWindow::applyStyleSheet()
{
    setStyleSheet(ui::styled(QStringLiteral(R"(
QWidget#windowSurface {
    background: {window}; border: 1px solid {border}; border-radius: 12px;
}
QWidget#titleBar { background: {surface}; border-top-left-radius: 12px; border-top-right-radius: 12px; }
QLabel#titleBarText { color: {text}; font-size: 13px; font-weight: 600; }
QToolButton#titleBarButton {
    border: none; border-radius: 6px; color: {textMuted}; font-size: 15px;
}
QToolButton#titleBarButton:hover { background: {surfaceHover}; }
QToolButton#titleBarClose {
    border: none; border-radius: 6px; color: {textMuted}; font-size: 14px;
}
QToolButton#titleBarClose:hover { background: {danger}; color: {surface}; }

QScrollArea#contentScroll { background: transparent; border: none; }
QWidget#contentArea { background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: {scrollbar}; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: {borderStrong}; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

QPushButton#actionCard {
    background: {surface}; border: 1px solid {border}; border-radius: 10px; text-align: left;
}
QPushButton#actionCard:hover { border-color: {borderStrong}; background: {surfaceHover}; }
QPushButton#actionCard:pressed { background: {surfaceHover}; }
QLabel#actionTitle { color: {text}; font-size: 14px; font-weight: 600; }
QLabel#hotkeyBadge { color: {textFaint}; font-size: 12px; }
QLabel#statusLabel { color: {textFaint}; font-size: 11px; }
QLabel#versionLabel { color: {textFaint}; font-size: 11px; }
)")));
}

void MainWindow::setHotkeyTexts(const QString& captureText, const QString& ocrText)
{
    const QString capture = captureText.isEmpty() ? QStringLiteral("F1") : captureText;
    const QString ocr = ocrText.isEmpty() ? QStringLiteral("F4") : ocrText;
    if (m_captureHotkeyBadge != nullptr) {
        m_captureHotkeyBadge->setText(capture);
    }
    if (m_ocrHotkeyBadge != nullptr) {
        m_ocrHotkeyBadge->setText(ocr);
    }
}

void MainWindow::setStatusText(const QString& text)
{
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(text);
    }
}

void MainWindow::setSettings(const common::AppSettings& settings)
{
    if (m_settingsPanel != nullptr) {
        m_settingsPanel->setSettings(settings);
    }
}

void MainWindow::revealSettings()
{
    if (m_scroll == nullptr || m_settingsSection == nullptr) {
        return;
    }
    // 等布局完成后再滚动，否则拿到的是旧的最大值。
    QTimer::singleShot(0, this, [this]() {
        m_scroll->ensureWidgetVisible(m_settingsSection, 0, 0);
        if (QScrollBar* bar = m_scroll->verticalScrollBar()) {
            bar->setValue(m_settingsSection->y());
        }
    });
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    // 首次显示时按内容真实高度定型：内容不多时不要滚动条，
    // 屏幕太矮时才退化为滚动。
    if (!m_heightAdjusted && m_contentWidget != nullptr && m_titleBar != nullptr) {
        const QScreen* screen = QGuiApplication::primaryScreen();
        const int availableHeight = screen != nullptr ? screen->availableGeometry().height() : 900;
        const int maxHeight = static_cast<int>(availableHeight * 0.88);
        // +14 留出内容区底部内边距与状态行的余量，避免最后一两行被裁。
        const int naturalHeight = kShadowMargin * 2 + m_titleBar->sizeHint().height()
                                  + m_contentWidget->sizeHint().height() + 14;
        setFixedHeight(qMin(naturalHeight, maxHeight));
        m_heightAdjusted = true;
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // 关闭时隐藏到托盘，由托盘菜单“退出”真正退出。
    event->ignore();
    hide();
}

}  // namespace snapocr::ui
