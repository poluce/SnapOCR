#include "app/Application.h"

#include <QKeyEvent>

#include <cstdio>
#include <cstdlib>
#include <memory>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCursor>
#include <windows.h>
#include <tlhelp32.h>

#include <QBuffer>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QTextEdit>
#include <QLoggingCategory>
#include <QDebug>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSysInfo>
#include <QFileInfo>
#include "capture/MonitorLayout.h"
#include <QStringList>
#include <functional>

#include <QTextStream>
#include <QTimer>

#include "app/HotkeyManager.h"
#include "app/ImageSaver.h"
#include "app/SingleInstance.h"
#include "app/TrayController.h"
#include "capture/CaptureCrop.h"
#include <QFont>
#include <QPainter>
#include <QEventLoop>
#include "capture/CaptureService.h"
#include "common/Logging.h"
#include "common/Settings.h"
#include "ocr/OcrHints.h"
#include "ocr/OcrService.h"
#include "ocr/WindowsOcrEngine.h"
#include "overlay/OverlayController.h"
#include "platform/win32/Clipboard.h"
#include "platform/win32/Startup.h"
#include "platform/win32/WindowActivation.h"
#include "snapocr/Version.h"
#include "history/HistoryStore.h"
#include "editor/AnnotationCanvas.h"
#include <QPixmap>
#include "update/UpdateChecker.h"
#include "ui/UpdateDialog.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QProcess>
#include "editor/EditorWindow.h"
#include "i18n/Translator.h"
#include "ui/HistoryDialog.h"
#include "ui/MainWindow.h"
#include "ui/OcrResultDialog.h"
#include "capture/ScrollingCaptureController.h"
#include "platform/win32/CrashReporter.h"
#include "platform/win32/ProcessMetrics.h"
#include "platform/win32/ScreenCapture.h"
#include "ui/PinWindow.h"
#include "ui/ScrollingBar.h"
#include "ui/Theme.h"

namespace snapocr::app {

namespace {

// 把虚拟桌面物理矩形换算成 Qt 逻辑矩形（逐屏匹配 DPR）。
QRect physicalRectToLogical(const QRect& physical)
{
    for (const QScreen* screen : QGuiApplication::screens()) {
        const QRect logical = screen->geometry();
        const qreal dpr = screen->devicePixelRatio();
        const QRect physicalGeometry(
            QPoint(qRound(logical.x() * dpr), qRound(logical.y() * dpr)),
            QSize(qRound(logical.width() * dpr), qRound(logical.height() * dpr)));
        if (physicalGeometry.intersects(physical)) {
            const QRect overlap = physicalGeometry.intersected(physical);
            return QRect(
                QPoint(logical.x() + qRound((overlap.x() - physicalGeometry.x()) / dpr),
                       logical.y() + qRound((overlap.y() - physicalGeometry.y()) / dpr)),
                QSize(qRound(overlap.width() / dpr), qRound(overlap.height() / dpr)));
        }
    }
    return physical;
}

// 逻辑矩形所在的屏幕（取第一个相交的屏幕，找不到就用主屏）。
QRect screenLogicalFor(const QRect& logicalRect)
{
    for (const QScreen* screen : QGuiApplication::screens()) {
        if (screen->geometry().intersects(logicalRect)) {
            return screen->geometry();
        }
    }
    if (const QScreen* primary = QGuiApplication::primaryScreen()) {
        return primary->geometry();
    }
    return QRect(0, 0, 1920, 1080);
}

// 把虚拟桌面物理坐标换算成 Qt 逻辑坐标（多屏不同 DPR 时逐屏匹配）。
QPoint physicalToLogicalPoint(const QPoint& physical)
{
    for (const QScreen* screen : QGuiApplication::screens()) {
        const QRect logical = screen->geometry();
        const qreal dpr = screen->devicePixelRatio();
        const QRect physicalGeometry(
            QPoint(qRound(logical.x() * dpr), qRound(logical.y() * dpr)),
            QSize(qRound(logical.width() * dpr), qRound(logical.height() * dpr)));
        if (physicalGeometry.contains(physical)) {
            return QPoint(logical.x() + qRound((physical.x() - physicalGeometry.x()) / dpr),
                          logical.y() + qRound((physical.y() - physicalGeometry.y()) / dpr));
        }
    }
    return physical;
}

}  // namespace

namespace {

constexpr auto kFlagHelp = "--help";
constexpr auto kFlagVersion = "--version";
constexpr auto kFlagSmokeTest = "--smoke-test";
constexpr auto kFlagCaptureSelfTest = "--capture-selftest";
constexpr auto kFlagStabilitySelfTest = "--stability-selftest";
constexpr auto kFlagPerfSelfTest = "--perf-selftest";
constexpr auto kFlagDoctor = "--doctor";
constexpr auto kFlagSelfTestAll = "--selftest-all";
constexpr auto kFlagUiPreview = "--ui-preview";
constexpr auto kFlagCheckUpdate = "--check-update";
constexpr auto kFlagCaptureDump = "--capture-dump";
constexpr auto kFlagDiagnose = "--diagnose";
constexpr auto kFlagUpdateFeed = "--update-feed";
constexpr auto kFlagCrashSelfTest = "--crash-selftest";
constexpr auto kFlagSettings = "--settings";

Q_LOGGING_CATEGORY(lcApp, "snapocr.app")

void printLine(const QString& text)
{
    QTextStream stream(stdout);
    stream << text << Qt::endl;
    stream.flush();
}

}  // namespace

Application::Application(QObject* parent)
    : QObject(parent)
{
}

Application::~Application() = default;

int Application::run(const QStringList& arguments)
{
    const QStringList args = arguments.mid(1);

    if (args.contains(QString::fromLatin1(kFlagVersion)) || args.contains(QStringLiteral("-v"))) {
        printVersion();
        return EXIT_SUCCESS;
    }
    if (args.contains(QString::fromLatin1(kFlagHelp)) || args.contains(QStringLiteral("-h"))) {
        printHelp();
        return EXIT_SUCCESS;
    }
    if (args.contains(QString::fromLatin1(kFlagSmokeTest))) {
        return runSmokeTest();
    }
    if (args.contains(QString::fromLatin1(kFlagCaptureSelfTest))) {
        return runCaptureSelfTest();
    }
    if (args.contains(QString::fromLatin1(kFlagStabilitySelfTest))) {
        return runStabilitySelfTest();
    }
    if (args.contains(QString::fromLatin1(kFlagPerfSelfTest))) {
        return runPerfSelfTest();
    }
    if (args.contains(QString::fromLatin1(kFlagDoctor))) {
        return runDoctor();
    }
    if (args.contains(QString::fromLatin1(kFlagSelfTestAll))) {
        return runSelfTestAll();
    }
    if (args.contains(QString::fromLatin1(kFlagDiagnose))) {
        const int diagnoseIndex = args.indexOf(QString::fromLatin1(kFlagDiagnose));
        QString diagnoseDir;
        if (diagnoseIndex + 1 < args.size() && !args.at(diagnoseIndex + 1).startsWith('-')) {
            diagnoseDir = args.at(diagnoseIndex + 1);
        }
        return runDiagnose(diagnoseDir);
    }
    if (args.contains(QString::fromLatin1(kFlagCaptureDump))) {
        const int dumpIndex = args.indexOf(QString::fromLatin1(kFlagCaptureDump));
        const QString dumpPath = (dumpIndex + 1 < args.size()) ? args.at(dumpIndex + 1) : QString();
        return runCaptureDump(dumpPath);
    }
    if (args.contains(QString::fromLatin1(kFlagCheckUpdate))) {
        QString feed;
        const int feedIndex = args.indexOf(QString::fromLatin1(kFlagUpdateFeed));
        if (feedIndex >= 0 && feedIndex + 1 < args.size()) {
            feed = args.at(feedIndex + 1);
        }
        return runUpdateCheckCommand(feed);
    }
    if (args.contains(QString::fromLatin1(kFlagUiPreview))) {
        const int index = args.indexOf(QString::fromLatin1(kFlagUiPreview));
        const QString output = (index + 1 < args.size()) ? args.at(index + 1) : QString();
        return runUiPreview(output);
    }
    if (args.contains(QString::fromLatin1(kFlagCrashSelfTest))) {
        // 崩溃记录自检：安装记录器后主动触发访问冲突，进程应写出 crash-*.txt
        platform::win32::installCrashReporter(common::defaultLogDirectory());
        ::RaiseException(0xC0000005, 0, 0, nullptr);
        return EXIT_FAILURE;
    }

    if (!initializeServices()) {
        // 已有实例在运行：命令已转发，本次启动直接退出。
        return EXIT_SUCCESS;
    }

    const bool startHidden = args.contains(QStringLiteral("--tray"));
    if (args.contains(QString::fromLatin1(kFlagSettings))) {
        showMainWindow();
        showSettings();
    } else if (!startHidden || m_tray == nullptr || !m_tray->isAvailable()) {
        showMainWindow();
    }

    return QApplication::exec();
}

bool Application::initializeServices()
{
    if (m_initialized) {
        return true;
    }
    m_initialized = true;

    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(QIcon());
    QGuiApplication::setApplicationDisplayName(QString::fromUtf8(snapocr::version::kAppName));

    m_settingsStore = std::make_unique<common::SettingsStore>();
    QString warning;
    m_settings = m_settingsStore->load(&warning);

    platform::win32::installCrashReporter(common::defaultLogDirectory());
    common::installLogging(m_settings.logLevel.isEmpty() ? common::defaultLogDirectory()
                                                         : common::defaultLogDirectory(),
                           m_settings.logLevel);
    ui::applyTheme(m_settings.theme);
    i18n::installUiTranslator(qobject_cast<QApplication*>(QApplication::instance()),
                              m_settings.language);

    if (!warning.isEmpty()) {
        qWarning("设置读取：%s", qUtf8Printable(warning));
    }

    // 单实例：已有实例时把命令转发过去并退出。
    m_singleInstance = new SingleInstance(this);
    QString instanceError;
    const bool primary = m_singleInstance->tryBecomePrimary(&instanceError);
    if (!instanceError.isEmpty()) {
        qWarning("单实例：%s", qUtf8Printable(instanceError));
    }
    if (!primary) {
        m_singleInstance->sendToPrimary(QStringLiteral("show"));
        return false;
    }
    connect(m_singleInstance, &SingleInstance::commandReceived, this,
            [this](const QString& command) {
                if (command == QLatin1String("capture")) {
                    startCapture();
                } else if (command == QLatin1String("ocr")) {
                    startOcrCapture();
                } else {
                    showMainWindow();
                }
            });

    m_tray = new TrayController(this);
    connect(m_tray, &TrayController::captureRequested, this, &Application::startCapture);
    connect(m_tray, &TrayController::ocrRequested, this, &Application::startOcrCapture);
    connect(m_tray, &TrayController::historyRequested, this, &Application::showHistory);
    connect(m_tray, &TrayController::pinLatestRequested, this,
            &Application::pinLatestFromHistory);
    connect(m_tray, &TrayController::settingsRequested, this, &Application::showSettings);
    connect(m_tray, &TrayController::showMainRequested, this, &Application::showMainWindow);
    connect(m_tray, &TrayController::quitRequested, this, &Application::quitApplication);
    connect(m_tray, &TrayController::updateCheckRequested, this,
            [this]() { checkForUpdates(true); });
    m_tray->show();

    // 在线更新（F-SYS-16）：启动后延迟几秒检查一次，避免影响启动速度与截图热键
    if (m_settings.updateCheckEnabled) {
        QTimer::singleShot(5000, this, [this]() { checkForUpdates(false); });
    }

    m_overlayController = new overlay::OverlayController(this);
    connect(m_overlayController, &overlay::OverlayController::copyRequested, this,
            &Application::onCopyRequested);
    connect(m_overlayController, &overlay::OverlayController::pinRequested, this,
            &Application::pinImage);
    connect(m_overlayController, &overlay::OverlayController::scrollingRequested, this,
            &Application::startScrollingCapture);
    connect(m_overlayController, &overlay::OverlayController::saveRequested, this,
            &Application::onSaveRequested);
    connect(m_overlayController, &overlay::OverlayController::ocrRequested, this,
            &Application::runOcr);
    connect(m_overlayController, &overlay::OverlayController::errorOccurred, this,
            &Application::onCaptureError);
    connect(m_overlayController, &overlay::OverlayController::colorPicked, this,
            [this](const QString& hex) {
                QString error;
                if (platform::win32::copyTextToClipboard(hex, &error)) {
                    showInfo(tr("已复制颜色 %1").arg(hex));
                } else {
                    showError(tr("复制颜色失败：%1").arg(error));
                }
            });

    connect(m_overlayController, &overlay::OverlayController::started, this, [this]() {
        registerOverlayHotkeys(true);
        if (m_captureLatencyTimer.isValid()) {
            qInfo("perf: 热键→遮罩 %lld ms", m_captureLatencyTimer.elapsed());
        }
    });
    connect(m_overlayController, &overlay::OverlayController::finished, this, [this]() {
        registerOverlayHotkeys(false);
        if (m_mainWindow != nullptr) {
            m_mainWindow->setStatusText(tr("就绪"));
        }
    });

    m_history = new history::HistoryStore(this);

    m_ocrService = new ocr::OcrService(this);
    m_ocrService->setEngine(std::make_unique<ocr::WindowsOcrEngine>());
    connect(m_ocrService, &ocr::OcrService::finished, this, &Application::onOcrFinished);

    m_hotkeys = new HotkeyManager(this);
    connect(m_hotkeys, &HotkeyManager::captureRequested, this, &Application::startCapture);
    connect(m_hotkeys, &HotkeyManager::ocrRequested, this, &Application::startOcrCapture);
    // 覆盖层期间的 Esc/Enter 由全局热键接住（窗口可能没有键盘焦点）。
    // 但如果当前有模态对话框（取色/线宽）或正在输入文字，热键会先于窗口收到按键，
    // 必须把按键转发给焦点控件，否则会出现“按 Enter 直接复制退出、文字丢失”。
    connect(m_hotkeys, &HotkeyManager::overlayCancelRequested, this,
            [this]() { forwardOverlayKey(Qt::Key_Escape); });
    connect(m_hotkeys, &HotkeyManager::overlayConfirmRequested, this,
            [this]() { forwardOverlayKey(Qt::Key_Return); });
    applyHotkeys();

    // 设置里的开机自启状态与注册表对齐（失败只记录，不阻塞启动）。
    QString startupError;
    platform::win32::setAutoStartEnabled(m_settings.startWithWindows, &startupError);
    if (!startupError.isEmpty()) {
        qWarning("开机自启：%s", qUtf8Printable(startupError));
    }

    m_mainWindow = std::make_unique<ui::MainWindow>();
    m_mainWindow->setSettings(m_settings);
    connect(m_mainWindow.get(), &ui::MainWindow::captureRequested, this,
            &Application::startCapture);
    connect(m_mainWindow.get(), &ui::MainWindow::ocrRequested, this, &Application::startOcrCapture);
    connect(m_mainWindow.get(), &ui::MainWindow::historyRequested, this,
            &Application::showHistory);
    connect(m_mainWindow.get(), &ui::MainWindow::settingsChanged, this,
            [this](const common::AppSettings& changed) { applySettings(changed); });
    updateHotkeyLabels();

    annotate::AnnotationStyle initialStyle;
    initialStyle.color = m_settings.lastColor;
    initialStyle.width = m_settings.lastWidth;
    initialStyle.fontSize = m_settings.lastFontSize;
    m_overlayController->setAnnotationStyle(initialStyle);
    m_overlayController->setIncludeCursor(m_settings.includeCursor);
    m_overlayController->setDetectWindows(m_settings.detectWindows);

    return true;
}

void Application::applyHotkeys()
{
    if (m_hotkeys == nullptr) {
        return;
    }

    QString captureError;
    const bool captureOk = m_hotkeys->registerHotkey(HotkeyManager::Action::Capture,
                                                     m_settings.captureHotkey, &captureError);
    qInfo("registration capture=%s seq=%s err=%s", captureOk ? "ok" : "FAILED",
          qUtf8Printable(m_settings.captureHotkey), qUtf8Printable(captureError));
    if (!captureOk) {
        // 回退到默认 F1，仍失败则提示用户去设置里改。
        const bool fallback = m_hotkeys->registerHotkey(HotkeyManager::Action::Capture,
                                                        QStringLiteral("F1"), nullptr);
        showError(fallback
                      ? tr("%1，已临时改用 F1。可在设置中更换截图热键。").arg(captureError)
                      : tr("%1，且默认 F1 也不可用。请在设置中更换截图热键。")
                            .arg(captureError));
    }

    QString ocrError;
    const bool ocrOk =
        m_hotkeys->registerHotkey(HotkeyManager::Action::Ocr, m_settings.ocrHotkey, &ocrError);
    qInfo("registration ocr=%s seq=%s err=%s", ocrOk ? "ok" : "FAILED",
          qUtf8Printable(m_settings.ocrHotkey), qUtf8Printable(ocrError));
    if (!ocrOk) {
        const bool fallback =
            m_hotkeys->registerHotkey(HotkeyManager::Action::Ocr, QStringLiteral("F4"), nullptr);
        showError(fallback
                      ? tr("%1，已临时改用 F4。可在设置中更换取字热键。").arg(ocrError)
                      : tr("%1，且默认 F4 也不可用。请在设置中更换取字热键。")
                            .arg(ocrError));
    }

    updateHotkeyLabels();
}

void Application::updateHotkeyLabels()
{
    const QString captureText = m_hotkeys != nullptr
                                    ? m_hotkeys->registeredText(HotkeyManager::Action::Capture)
                                    : QStringLiteral("F1");
    const QString ocrText = m_hotkeys != nullptr
                                ? m_hotkeys->registeredText(HotkeyManager::Action::Ocr)
                                : QStringLiteral("F4");
    if (m_mainWindow != nullptr) {
        m_mainWindow->setHotkeyTexts(captureText, ocrText);
    }
    if (m_tray != nullptr) {
        m_tray->setHotkeyTexts(captureText, ocrText);
    }
}

// 决定覆盖层期间收到的 Esc/Enter 该给谁：
// 1) 有模态对话框 → 转发给对话框；
// 2) 焦点在文本输入控件（文字标注的 QTextEdit）→ 转发，Enter 换行、Esc 取消输入；
// 3) 否则才当作“取消/确认截图”。
void Application::forwardOverlayKey(int key)
{
    // 滚动长截图期间：Esc 取消、Enter 完成
    if (m_scrolling != nullptr && m_scrolling->isActive()) {
        if (key == Qt::Key_Escape) {
            cancelScrollingCapture();
        } else {
            finishScrollingCapture();
        }
        return;
    }

    auto sendTo = [key](QWidget* target) {
        if (target == nullptr) {
            return;
        }
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier,
                        key == Qt::Key_Return ? QStringLiteral("\r") : QString());
        QApplication::sendEvent(target, &press);
    };

    if (QWidget* modal = QApplication::activeModalWidget(); modal != nullptr) {
        sendTo(modal);
        return;
    }

    if (QWidget* focus = QApplication::focusWidget(); focus != nullptr) {
        // 文本类输入控件（文字标注 / 任何行编辑）自己处理 Enter、Esc
        if (qobject_cast<QTextEdit*>(focus) != nullptr
            || qobject_cast<QLineEdit*>(focus) != nullptr
            || qobject_cast<QAbstractSpinBox*>(focus) != nullptr) {
            sendTo(focus);
            return;
        }
    }

    if (m_overlayController == nullptr) {
        return;
    }
    if (key == Qt::Key_Escape) {
        m_overlayController->requestCancel();
    } else {
        m_overlayController->requestConfirm();
    }
}

void Application::registerOverlayHotkeys(bool enabled)
{
    if (m_hotkeys == nullptr) {
        return;
    }
    if (enabled) {
        // 覆盖层显示期间临时占用 Esc / Enter：窗口没焦点时也能退出或确认。
        // 注册失败不阻塞流程（例如按键已被其他程序占用），覆盖层仍可用鼠标操作。
        m_hotkeys->registerHotkey(HotkeyManager::Action::OverlayCancel, QStringLiteral("Esc"),
                                  nullptr);
        m_hotkeys->registerHotkey(HotkeyManager::Action::OverlayConfirm, QStringLiteral("Enter"),
                                  nullptr);
    } else {
        m_hotkeys->unregisterHotkey(HotkeyManager::Action::OverlayCancel);
        m_hotkeys->unregisterHotkey(HotkeyManager::Action::OverlayConfirm);
    }
}

void Application::startCapture()
{
    m_captureLatencyTimer.start();
    qCDebug(lcApp, "startCapture requested delay=%d", m_settings.delaySeconds);
    if (m_overlayController == nullptr) {
        return;
    }
    if (m_overlayController->isActive()) {
        // 已在进行中：先取消再重开，避免“热键没反应”的困惑。
        m_overlayController->cancel();
    }

    const int delay = m_settings.delaySeconds;
    if (delay > 0 && !m_delayPending) {
        // 延时截图：先倒计时提示，到点再抓屏（用于展开菜单、等待动画等场景）。
        m_delayPending = true;
        m_delayOcrMode = false;
        if (m_tray != nullptr && m_tray->isAvailable()) {
            m_tray->showMessage(QStringLiteral("SnapOCR"),
                                tr("%1 秒后截图…").arg(delay));
        }
        QTimer::singleShot(delay * 1000, this, [this]() {
            m_delayPending = false;
            m_captureLatencyTimer.restart();  // 延时到点后重新计时（不含用户设置的等待）
            beginCaptureNow(m_delayOcrMode);
        });
        return;
    }
    if (m_delayPending) {
        return;  // 倒计时期间忽略重复触发
    }

    beginCaptureNow(false);
}

void Application::beginCaptureNow(bool ocrMode)
{
    if (m_overlayController == nullptr) {
        return;
    }
    if (m_delayPending) {
        m_delayPending = false;
    }
    if (m_mainWindow != nullptr) {
        m_mainWindow->setStatusText(ocrMode ? tr("取字：拖拽选择区域")
                                            : tr("截图：拖拽选择区域"));
    }
    if (ocrMode) {
        m_overlayController->beginOcrCapture();
    } else {
        m_overlayController->beginCapture();
    }
}

void Application::startOcrCapture()
{
    qCDebug(lcApp, "startOcrCapture requested");
    if (m_overlayController == nullptr) {
        return;
    }
    if (m_overlayController->isActive()) {
        m_overlayController->cancel();
    }

    const int delay = m_settings.delaySeconds;
    if (delay > 0 && !m_delayPending) {
        m_delayPending = true;
        m_delayOcrMode = true;
        if (m_tray != nullptr && m_tray->isAvailable()) {
            m_tray->showMessage(QStringLiteral("SnapOCR"),
                                tr("%1 秒后取字…").arg(delay));
        }
        QTimer::singleShot(delay * 1000, this, [this]() {
            m_delayPending = false;
            m_captureLatencyTimer.restart();  // 延时到点后重新计时（不含用户设置的等待）
            beginCaptureNow(m_delayOcrMode);
        });
        return;
    }
    if (m_delayPending) {
        return;
    }
    beginCaptureNow(true);
}

void Application::recordHistory(const QImage& image)
{
    if (m_history == nullptr || image.isNull()) {
        return;
    }
    m_history->add(image);
    if (m_historyDialog != nullptr) {
        m_historyDialog->refresh();
    }
}

void Application::startScrollingCapture(const QRect& physicalRect)
{
    if (physicalRect.isEmpty()) {
        return;
    }
    if (m_scrolling == nullptr) {
        m_scrolling = new capture::ScrollingCaptureController(this);
        m_scrolling->setFrameProvider([this](QImage* image, QString* error) {
            return platform::win32::ScreenCapture::captureRegion(m_scrollingRegion, image, error);
        });
        // 注意：lambda 连接不支持 Qt::UniqueConnection，必须在创建时只连一次；
        // 否则每次开始长截图都会再连一份，导致 finished 被多次处理。
        connect(m_scrolling, &capture::ScrollingCaptureController::finished, this,
                [this](const QImage& image) {
                    if (m_scrollingBar != nullptr) {
                        m_scrollingBar->close();
                    }
                    registerOverlayHotkeys(false);
                    recordHistory(image);
                    QString error;
                    if (platform::win32::copyImageToClipboard(image, &error)) {
                        showInfo(tr("长截图完成（%1 × %2），已复制到剪贴板")
                                     .arg(image.width())
                                     .arg(image.height()));
                    } else {
                        showInfo(tr("长截图完成（%1 × %2），但复制失败：%3")
                                     .arg(image.width())
                                     .arg(image.height())
                                     .arg(error));
                    }
                });
        connect(m_scrolling, &capture::ScrollingCaptureController::failed, this,
                [this](const QString& message) {
                    if (m_scrollingBar != nullptr) {
                        m_scrollingBar->close();
                    }
                    registerOverlayHotkeys(false);
                    showError(tr("长截图失败：%1").arg(message));
                });
        connect(m_scrolling, &capture::ScrollingCaptureController::progress, this,
                [this](int frames, int height) {
                    if (m_scrollingBar != nullptr) {
                        m_scrollingBar->setProgress(frames, height);
                    }
                });
    }
    // 记录区域：provider 是静态 lambda，用成员变量传递当前区域
    m_scrollingRegion = physicalRect;

    if (m_scrollingBar == nullptr) {
        auto* bar = new ui::ScrollingBar();
        bar->setAttribute(Qt::WA_DeleteOnClose);
        connect(bar, &ui::ScrollingBar::finishRequested, this,
                &Application::finishScrollingCapture);
        connect(bar, &ui::ScrollingBar::cancelRequested, this,
                &Application::cancelScrollingCapture);
        connect(bar, &QObject::destroyed, this, [this]() { m_scrollingBar = nullptr; });
        m_scrollingBar = bar;
    }


    // 期间用 Esc/Enter 控制：复用覆盖层的全局热键（窗口没有焦点也能用）
    registerOverlayHotkeys(true);

    m_scrolling->start(physicalRect);
    if (m_scrollingBar != nullptr) {
        // 控制条必须落在截取区域之外，否则会被抓进长图里。
        // 依次尝试：区域下方 → 区域上方 → 屏幕左上角，取第一个不与区域相交的位置。
        const QSize barSize = m_scrollingBar->size();
        const QRect regionLogical = physicalRectToLogical(m_scrollingRegion);
        const QRect screenLogical = screenLogicalFor(regionLogical);
        QPoint position = regionLogical.topLeft() + QPoint(0, regionLogical.height() + 12);
        const QRect belowRect(position, barSize);
        if (!belowRect.intersects(regionLogical) && screenLogical.contains(belowRect)) {
            m_scrollingBar->move(position);
        } else {
            position = QPoint(regionLogical.left(), regionLogical.top() - barSize.height() - 12);
            const QRect aboveRect(position, barSize);
            if (!aboveRect.intersects(regionLogical) && screenLogical.contains(aboveRect)) {
                m_scrollingBar->move(position);
            } else {
                // 区域占满整屏：放到屏幕上边角，并保证不与区域相交已不可能，
                // 此时让控制条半透明，尽量减少对截图的影响。
                m_scrollingBar->move(screenLogical.topLeft() + QPoint(12, 12));
                m_scrollingBar->setWindowOpacity(0.85);
            }
        }
        m_scrollingBar->show();
        m_scrollingBar->raise();
    }
}

void Application::finishScrollingCapture()
{
    if (m_scrolling != nullptr && m_scrolling->isActive()) {
        m_scrolling->finish();
    }
}

void Application::cancelScrollingCapture()
{
    if (m_scrolling != nullptr && m_scrolling->isActive()) {
        m_scrolling->cancel();
    }
    if (m_scrollingBar != nullptr) {
        m_scrollingBar->close();
    }
    registerOverlayHotkeys(false);
    showInfo(tr("已取消长截图"));
}

void Application::openEditorForImage(const QImage& image)
{
    if (image.isNull()) {
        return;
    }
    if (m_pinEditor != nullptr) {
        m_pinEditor->close();
        m_pinEditor = nullptr;
    }

    m_pinEditorDocument = std::make_unique<annotate::AnnotationDocument>();
    m_pinEditorDocument->setBaseImage(image);

    auto* editor = new editor::EditorWindow;
    editor->setAttribute(Qt::WA_DeleteOnClose);
    connect(editor, &QObject::destroyed, this, [this]() {
        m_pinEditor = nullptr;
        m_pinEditorDocument.reset();
    });
    editor->setDocument(m_pinEditorDocument.get());

    annotate::AnnotationStyle style;
    style.color = m_settings.lastColor;
    style.width = qMax(1, m_settings.lastWidth);
    style.fontSize = qMax(6, m_settings.lastFontSize);
    editor->setStyle(style);
    editor->setTool(annotate::AnnotationKind::Rectangle);

    connect(editor, &editor::EditorWindow::confirmRequested, this, [this, editor]() {
        onCopyRequested(editor->render());
        editor->close();
    });
    connect(editor, &editor::EditorWindow::saveRequested, this, [this, editor]() {
        onSaveRequested(editor->render());
    });
    connect(editor, &editor::EditorWindow::cancelRequested, this, [editor]() {
        editor->close();
    });

    // 定位：以光标为中心、按屏幕 DPR 换算逻辑尺寸，夹取到屏幕内
    const qreal dpr = QGuiApplication::primaryScreen() != nullptr
                          ? QGuiApplication::primaryScreen()->devicePixelRatio()
                          : 1.0;
    const QSize logicalSize(qRound(image.width() / dpr), qRound(image.height() / dpr));
    const QRect screen = screenLogicalFor(QRect(QCursor::pos(), QSize(1, 1)));
    QRect geometry(QPoint(0, 0), logicalSize);
    geometry.moveCenter(QCursor::pos());
    if (geometry.left() < screen.left()) {
        geometry.moveLeft(screen.left());
    }
    if (geometry.top() < screen.top()) {
        geometry.moveTop(screen.top());
    }
    if (geometry.right() > screen.right()) {
        geometry.moveRight(screen.right());
    }
    if (geometry.bottom() > screen.bottom()) {
        geometry.moveBottom(screen.bottom());
    }
    editor->placeAt(geometry);
    editor->show();
    platform::win32::forceForegroundWindow(editor);
    if (editor->canvas() != nullptr) {
        editor->canvas()->setFocus();
    }

    m_pinEditor = editor;
    if (m_mainWindow != nullptr) {
        m_mainWindow->setStatusText(tr("贴图再标注：Enter 复制，Ctrl+S 保存，Esc 关闭"));
    }
}

void Application::pinLatestFromHistory()
{
    if (m_history == nullptr) {
        return;
    }
    const QVector<history::HistoryEntry> entries = m_history->entries();
    if (entries.isEmpty()) {
        showInfo(tr("还没有历史截图：先截一张再试"));
        return;
    }
    const QImage image = m_history->load(entries.first());
    if (image.isNull()) {
        showError(tr("历史截图读取失败"));
        return;
    }
    pinImage(image);
}

void Application::showHistory()
{
    if (m_history == nullptr) {
        return;
    }
    if (m_historyDialog == nullptr) {
        auto* dialog = new ui::HistoryDialog(m_history);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &ui::HistoryDialog::copyRequested, this, &Application::onCopyRequested);
        connect(dialog, &ui::HistoryDialog::pinRequested, this, &Application::pinImage);
        connect(dialog, &ui::HistoryDialog::saveRequested, this, &Application::onSaveRequested);
        m_historyDialog = dialog;
    }
    m_historyDialog->refresh();
    m_historyDialog->show();
    m_historyDialog->raise();
    m_historyDialog->activateWindow();
}

void Application::pinImage(const QImage& image)
{
    if (image.isNull()) {
        return;
    }
    recordHistory(image);
    auto* pin = new ui::PinWindow(image);
    // 贴图窗口出现在光标附近，避免正好盖住选区原位置影响对照。
    const QPoint cursor = QCursor::pos();
    pin->move(cursor + QPoint(24, 24));
    pin->show();

    connect(pin, &ui::PinWindow::copyRequested, this, [this](const QImage& img) {
        onCopyRequested(img);
    });
    connect(pin, &ui::PinWindow::saveRequested, this, [this](const QImage& img) {
        onSaveRequested(img);
    });
    // F-ANN-19：贴图可再取字 / 再标注
    connect(pin, &ui::PinWindow::ocrRequested, this, [this](const QImage& img) {
        if (m_mainWindow != nullptr) {
            m_mainWindow->setStatusText(tr("贴图取字：识别中…"));
        }
        runOcr(img);
    });
    connect(pin, &ui::PinWindow::editRequested, this, [this](const QImage& img) {
        openEditorForImage(img);
    });
    connect(pin, &QObject::destroyed, this, [this, pin]() {
        // 析构期间不要再用 QPointer(pin) 构造（对象已处于销毁中），按裸指针过滤
        for (int i = m_pins.size() - 1; i >= 0; --i) {
            if (m_pins.at(i).data() == pin || m_pins.at(i).isNull()) {
                m_pins.removeAt(i);
            }
        }
    });

    m_pins.append(QPointer<ui::PinWindow>(pin));
    if (m_mainWindow != nullptr) {
        m_mainWindow->setStatusText(tr("已贴图（滚轮缩放，R 旋转，Esc 关闭）"));
    }
}

void Application::onCopyRequested(const QImage& image)
{
    recordHistory(image);
    QString error;
    if (platform::win32::copyImageToClipboard(image, &error)) {
        showInfo(tr("已复制到剪贴板（%1 × %2）").arg(image.width()).arg(image.height()));
    } else {
        showError(tr("复制失败：%1").arg(error));
    }

    // F-ANN-20：可配置的“一键复制并贴图”
    if (m_settings.pinOnCopy) {
        pinImage(image);
    }
}

void Application::onSaveRequested(const QImage& image)
{
    recordHistory(image);
    QString savedPath;
    QString error;
    if (ImageSaver::save(image, m_settings, &savedPath, &error)) {
        showInfo(tr("已保存：%1").arg(QDir::toNativeSeparators(savedPath)));
    } else {
        showError(tr("保存失败：%1").arg(error));
    }
}

void Application::onCaptureError(const QString& message)
{
    showError(message);
}

void Application::runOcr(const QImage& image)
{
    if (image.isNull()) {
        showError(tr("没有可识别的图像"));
        return;
    }

    m_lastOcrImage = image;
    m_lastOcrSelectionSize = image.size();
    m_lastOcrTouchesEdge =
        m_overlayController != nullptr && m_overlayController->lastSelectionTouchesScreenEdge();

    ui::OcrResultDialog* dialog = ensureOcrDialog();
    dialog->setBusy(tr("正在识别…（离线本地识别）"));
    dialog->show();
    dialog->raise();
    dialog->activateWindow();

    if (m_ocrService == nullptr) {
        return;
    }
    if (m_ocrService->isBusy()) {
        dialog->setResult(ocr::OcrResult{.ok = false,
                                         .errorMessage = tr("上一次识别还没结束，请稍候")});
        return;
    }
    m_ocrService->recognizeAsync(image, m_settings.ocrLanguage);
}

ui::OcrResultDialog* Application::ensureOcrDialog()
{
    if (m_ocrDialog != nullptr) {
        return m_ocrDialog;
    }

    auto* dialog = new ui::OcrResultDialog(m_mainWindow.get());
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(dialog, &ui::OcrResultDialog::copyRequested, this, [this](const QString&) {
        showInfo(tr("识别文本已复制到剪贴板"));
    });
    connect(dialog, &ui::OcrResultDialog::retryRequested, this, [this, dialog]() {
        if (!m_lastOcrImage.isNull()) {
            const QImage image = m_lastOcrImage;
            dialog->setBusy(tr("正在重新识别…"));
            m_ocrService->recognizeAsync(image, m_settings.ocrLanguage);
        }
    });
    m_ocrDialog = dialog;
    return dialog;
}

void Application::onOcrFinished(const ocr::OcrResult& result)
{
    ui::OcrResultDialog* dialog = ensureOcrDialog();
    const QString problem =
        ocr::describeOcrProblem(result, m_lastOcrSelectionSize, m_lastOcrTouchesEdge);
    dialog->setResult(result, problem);
    dialog->show();
    dialog->raise();

    // F-OCR-09：把引擎错误翻译成可执行的提示（语言包缺失/选区太小/贴边裁字/超时）
    if (!result.ok) {
        showError(problem);
        return;
    }

    const QString text = result.fullText.trimmed();
    if (text.isEmpty()) {
        showInfo(problem);
        return;
    }

    const QString copyText = result.cleanText.isEmpty() ? result.fullText : result.cleanText;
    if (m_settings.ocrAutoCopy) {
        QApplication::clipboard()->setText(copyText);
        showInfo(tr("已识别 %1 行文字并复制到剪贴板（%2ms）")
                     .arg(result.lines.size())
                     .arg(result.elapsedMs));
    } else {
        showInfo(tr("已识别 %1 行文字（%2ms）")
                     .arg(result.lines.size())
                     .arg(result.elapsedMs));
    }
}

void Application::showMainWindow()
{
    if (m_mainWindow == nullptr) {
        return;
    }
    // 首次显示或窗口跑到屏幕外时，居中到主屏可用区域。
    const QScreen* screen = QGuiApplication::primaryScreen();
    if (screen != nullptr) {
        const QRect available = screen->availableGeometry();
        if (!available.intersects(m_mainWindow->frameGeometry())) {
            const QSize size = m_mainWindow->size();
            m_mainWindow->move(available.center().x() - size.width() / 2,
                               available.center().y() - size.height() / 2);
        }
    }
    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
    // 从后台/托盘唤起时，系统前台锁可能不让窗口到最前，强制一次。
    platform::win32::forceForegroundWindow(m_mainWindow.get());
}

void Application::showSettings()
{
    // 设置已合并进主窗口：只需显示窗口并滚动到设置区域，改动即时生效。
    showMainWindow();
    if (m_mainWindow != nullptr) {
        m_mainWindow->revealSettings();
    }
}

// 设置面板任何改动都会走这里：比较差异 -> 应用 -> 持久化。
// 热键注册失败会回滚到旧值并提示（避免用户改成一个被占用的键后无法截图）。
void Application::applySettings(const common::AppSettings& incoming)
{
    const common::AppSettings previous = m_settings;
    m_settings = incoming;

    if (m_settings.captureHotkey != previous.captureHotkey
        || m_settings.ocrHotkey != previous.ocrHotkey) {
        QString error;
        bool rollback = false;
        if (m_settings.captureHotkey != previous.captureHotkey
            && !m_hotkeys->registerHotkey(HotkeyManager::Action::Capture, m_settings.captureHotkey,
                                          &error)) {
            showError(error);
            m_settings.captureHotkey = previous.captureHotkey;
            rollback = true;
        }
        if (m_settings.ocrHotkey != previous.ocrHotkey
            && !m_hotkeys->registerHotkey(HotkeyManager::Action::Ocr, m_settings.ocrHotkey,
                                          &error)) {
            showError(error);
            m_settings.ocrHotkey = previous.ocrHotkey;
            rollback = true;
        }
        // 保证两套热键最终都处于已注册状态。
        m_hotkeys->registerHotkey(HotkeyManager::Action::Capture, m_settings.captureHotkey, nullptr);
        m_hotkeys->registerHotkey(HotkeyManager::Action::Ocr, m_settings.ocrHotkey, nullptr);
        updateHotkeyLabels();
        if (rollback) {
            // 回滚后把面板显示值同步回真实值，避免界面与实际不一致。
            if (m_mainWindow != nullptr) {
                m_mainWindow->setSettings(m_settings);
            }
            return;
        }
    }

    if (m_overlayController != nullptr) {
        m_overlayController->setIncludeCursor(m_settings.includeCursor);
    m_overlayController->setDetectWindows(m_settings.detectWindows);
    }

    if (m_settings.theme != previous.theme) {
        ui::applyTheme(m_settings.theme);
    }
    if (m_settings.language != previous.language) {
        // 翻译器切换后文字由 Qt 在下一次界面重建/语言事件时生效；此处只负责加载
        i18n::installUiTranslator(qobject_cast<QApplication*>(QApplication::instance()),
                                  m_settings.language);
    }

    if (m_settings.startWithWindows != previous.startWithWindows) {
        QString error;
        if (!platform::win32::setAutoStartEnabled(m_settings.startWithWindows, &error)) {
            showError(error);
        }
    }

    if (m_settings.logLevel != previous.logLevel) {
        common::installLogging(common::defaultLogDirectory(), m_settings.logLevel);
    }

    annotate::AnnotationStyle style;
    style.color = m_settings.lastColor;
    style.width = m_settings.lastWidth;
    style.fontSize = m_settings.lastFontSize;
    if (m_overlayController != nullptr) {
        m_overlayController->setAnnotationStyle(style);
    }

    persistSettings();
    if (m_mainWindow != nullptr) {
        m_mainWindow->setStatusText(tr("设置已保存"));
    }
}

void Application::persistSettings()
{
    if (m_settingsStore == nullptr) {
        return;
    }
    QString error;
    if (!m_settingsStore->save(m_settings, &error)) {
        showError(tr("设置保存失败：%1").arg(error));
    }
}

void Application::quitApplication()
{
    persistSettings();
    if (m_hotkeys != nullptr) {
        m_hotkeys->unregisterAll();
    }
    if (m_tray != nullptr) {
        m_tray->hide();
    }
    QApplication::quit();
}

bool Application::captureDumpTo(const QString& filePath, QTextStream& out)
{

    out << "SnapOCR capture dump\n";
    if (filePath.trimmed().isEmpty()) {
        out << "FAIL: 缺少输出文件路径\n";
        out.flush();
        return false;
    }

    capture::CaptureService service;
    capture::CaptureResult result;
    QString error;
    QEventLoop loop;
    QObject::connect(&service, &capture::CaptureService::captured, &loop,
                     [&result, &loop](const capture::CaptureResult& captured) {
                         result = captured;
                         loop.quit();
                     });
    QObject::connect(&service, &capture::CaptureService::failed, &loop,
                     [&error, &loop](const QString& message) {
                         error = message;
                         loop.quit();
                     });
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10000);
    service.captureAllScreens();
    loop.exec();

    if (!result.isValid()) {
        out << "FAIL: " << (error.isEmpty() ? tr("捕获失败") : error) << '\n';
        out.flush();
        return false;
    }

    // 拼成整个虚拟桌面（物理像素 1:1），便于检查窗口是否被包含
    QImage combined(result.virtualGeometry.size(), QImage::Format_ARGB32);
    combined.fill(Qt::black);
    for (const capture::ScreenFrame& frame : result.screens) {
        const QPoint offset = frame.monitor.physicalGeometry.topLeft()
                              - result.virtualGeometry.topLeft();
        QPainter painter(&combined);
        painter.drawImage(offset, frame.image);
    }
    if (!combined.save(filePath, "PNG")) {
        out << "FAIL: 无法写入 " << filePath << '\n';
        out.flush();
        return false;
    }
    out << "  virtual : " << result.virtualGeometry.width() << 'x'
        << result.virtualGeometry.height() << '\n';
    out << "  screens : " << result.screens.size() << '\n';
    for (const capture::ScreenFrame& frame : result.screens) {
        out << "    - " << frame.monitor.id << " phys=(" << frame.monitor.physicalGeometry.x()
            << ',' << frame.monitor.physicalGeometry.y() << ' '
            << frame.monitor.physicalGeometry.width() << 'x'
            << frame.monitor.physicalGeometry.height() << ") logical=("
            << frame.monitor.logicalGeometry.x() << ',' << frame.monitor.logicalGeometry.y()
            << ' ' << frame.monitor.logicalGeometry.width() << 'x'
            << frame.monitor.logicalGeometry.height() << ") dpr=" << frame.monitor.devicePixelRatio
            << " image=" << frame.image.width() << 'x' << frame.image.height() << '\n';
    }
    // 每块屏单独存一份：便于对照“哪块屏没抓到/抓错”
    const QFileInfo fileInfo(filePath);
    const QString baseName = fileInfo.completeBaseName();
    const QString suffix = fileInfo.suffix().isEmpty() ? QStringLiteral("png") : fileInfo.suffix();
    int index = 0;
    for (const capture::ScreenFrame& frame : result.screens) {
        const QString monitorPath =
            fileInfo.dir().filePath(QStringLiteral("%1.m%2.%3").arg(baseName).arg(index).arg(suffix));
        if (frame.image.save(monitorPath, "PNG")) {
            out << "    saved monitor " << index << " : " << monitorPath << '\n';
        }
        ++index;
    }
    out << "  saved   : " << filePath << '\n';
    out << "PASS\n";
    out.flush();
    return true;
}

int Application::runCaptureDump(const QString& filePath)
{
    QTextStream out(stdout);
    const bool ok = captureDumpTo(filePath, out);
    out.flush();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int Application::runDiagnose(const QString& outputDirectory)
{
    QString directory = outputDirectory.trimmed();
    if (directory.isEmpty()) {
        directory = QDir(QCoreApplication::applicationDirPath())
                        .filePath(QStringLiteral("diagnose-%1")
                                      .arg(QDateTime::currentDateTime().toString(
                                          QStringLiteral("yyyyMMdd-HHmmss"))));
    }
    if (!QDir().mkpath(directory)) {
        QTextStream(stdout) << "FAIL: 无法创建诊断目录：" << directory << '\n';
        return EXIT_FAILURE;
    }

    // 1) 环境/显示器报告
    const QString doctorPath = QDir(directory).filePath(QStringLiteral("doctor.txt"));
    {
        QFile file(doctorPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream(stdout) << "FAIL: 无法写入 " << doctorPath << '\n';
            return EXIT_FAILURE;
        }
        QTextStream out(&file);
        out << buildDoctorReport();
        out.flush();
    }

    // 2) 多屏捕获证据（整屏 + 每屏单独 PNG + 明细）
    const QString dumpPath = QDir(directory).filePath(QStringLiteral("dump.png"));
    const QString captureReportPath = QDir(directory).filePath(QStringLiteral("capture.txt"));
    bool captureOk = false;
    {
        QFile file(captureReportPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream(stdout) << "FAIL: 无法写入 " << captureReportPath << '\n';
            return EXIT_FAILURE;
        }
        QTextStream out(&file);
        captureOk = captureDumpTo(dumpPath, out);
        out.flush();
    }

    QTextStream out(stdout);
    out << "SnapOCR diagnose\n";
    out << "  directory : " << directory << '\n';
    out << "  doctor    : " << doctorPath << '\n';
    out << "  capture   : " << (captureOk ? "ok" : "FAILED") << " -> " << captureReportPath << '\n';
    out << (captureOk ? "PASS\n" : "FAIL\n");
    out.flush();
    return captureOk ? EXIT_SUCCESS : EXIT_FAILURE;
}

int Application::runUpdateCheckCommand(const QString& feedUrl)
{
    QTextStream out(stdout);
    out << "SnapOCR update check\n";

    update::UpdateChecker checker;
    checker.setCurrentVersion(QString::fromUtf8(version::kVersion));
    checker.setFeedUrl(feedUrl.trimmed().isEmpty()
                           ? update::UpdateChecker::defaultFeedUrl()
                           : QUrl(feedUrl.trimmed()));
    out << "  current      : " << QString::fromUtf8(version::kVersion) << '\n';
    out << "  feed         : " << checker.feedUrl().toString() << '\n';
    out.flush();

    QEventLoop loop;
    bool ok = false;
    bool newer = false;
    update::UpdateRelease release;
    QString error;
    connect(&checker, &update::UpdateChecker::finished, &loop,
            [&ok, &newer, &release, &error, &loop](bool success,
                                                  const update::UpdateRelease& found,
                                                  bool hasNewer, const QString& message) {
                ok = success;
                newer = hasNewer;
                release = found;
                error = message;
                loop.quit();
            });
    checker.checkAsync();
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(update::UpdateChecker::kDefaultTimeoutMs + 5000);
    loop.exec();

    if (!ok) {
        out << "FAIL: " << (error.isEmpty() ? tr("检查更新超时") : error) << '\n';
        out.flush();
        return EXIT_FAILURE;
    }
    out << "  latest       : " << release.version << '\n';
    out << "  installer    : "
        << (release.hasInstaller() ? release.setupUrl.toString() : QStringLiteral("(none)"))
        << '\n';
    out << "  notes        : " << release.notes.left(200).replace('\n', QStringLiteral(" ")) << '\n';
    out << (newer ? "UPDATE_AVAILABLE\n" : "UP_TO_DATE\n");
    out.flush();
    return EXIT_SUCCESS;
}

void Application::checkForUpdates(bool interactive)
{
    m_updateCheckInteractive = interactive;
    if (m_updateChecker == nullptr) {
        m_updateChecker = new update::UpdateChecker(this);
        connect(m_updateChecker, &update::UpdateChecker::finished, this,
                [this](bool ok, const update::UpdateRelease& release, bool newerAvailable,
                       const QString& errorMessage) {
                    if (!ok) {
                        if (m_updateCheckInteractive) {
                            showError(tr("检查更新失败：%1").arg(errorMessage));
                        } else {
                            qInfo("update check failed: %s", qUtf8Printable(errorMessage));
                        }
                        return;
                    }
                    if (!newerAvailable) {
                        if (m_updateCheckInteractive) {
                            showInfo(tr("已是最新版本（%1）")
                                         .arg(QString::fromUtf8(version::kVersion)));
                        }
                        return;
                    }
                    if (release.version == m_settings.skippedUpdateVersion) {
                        qInfo("update %s skipped by user", qUtf8Printable(release.version));
                        if (m_updateCheckInteractive) {
                            showInfo(tr("已跳过 v%1，可在托盘菜单重新检查")
                                         .arg(release.version));
                        }
                        return;
                    }
                    qInfo("update available: %s", qUtf8Printable(release.version));
                    promptForUpdate(release);
                });
    }
    m_updateChecker->setCurrentVersion(QString::fromUtf8(version::kVersion));
    m_updateChecker->setFeedUrl(m_settings.updateFeedUrl.trimmed().isEmpty()
                                    ? update::UpdateChecker::defaultFeedUrl()
                                    : QUrl(m_settings.updateFeedUrl.trimmed()));
    if (m_updateChecker->isChecking()) {
        return;
    }
    if (interactive) {
        showInfo(tr("正在检查更新…"));
    }
    m_updateChecker->checkAsync();
}

void Application::promptForUpdate(const update::UpdateRelease& release)
{
    if (m_tray != nullptr && m_tray->isAvailable()) {
        m_tray->showMessage(QStringLiteral("SnapOCR"),
                            tr("发现新版本 %1，点击托盘菜单「检查更新」查看").arg(release.version));
    }

    ui::UpdateDialog dialog(release, QString::fromUtf8(version::kVersion));
    dialog.exec();
    switch (dialog.choice()) {
    case ui::UpdateDialog::Choice::DownloadAndInstall:
        startUpdateDownload(release);
        break;
    case ui::UpdateDialog::Choice::SkipVersion:
        m_settings.skippedUpdateVersion = release.version;
        if (m_settingsStore != nullptr) {
            m_settingsStore->save(m_settings, nullptr);
        }
        showInfo(tr("已跳过 v%1").arg(release.version));
        break;
    case ui::UpdateDialog::Choice::RemindLater:
        break;
    }
}

void Application::startUpdateDownload(const update::UpdateRelease& release)
{
    if (m_updateDownloading) {
        return;
    }
    if (!release.hasInstaller()) {
        // 没有安装包时退化为打开发布页，由用户选择下载方式
        const QUrl target = release.htmlUrl.isEmpty()
                                ? QUrl(QStringLiteral("https://github.com/poluce/SnapOCR/releases"))
                                : QUrl(release.htmlUrl);
        QDesktopServices::openUrl(target);
        showInfo(tr("已在浏览器打开下载页面"));
        return;
    }

    if (m_updateNetwork == nullptr) {
        m_updateNetwork = new QNetworkAccessManager(this);
    }
    const QString directory =
        QDir(QDir::tempPath()).filePath(QStringLiteral("SnapOCR-update"));
    QDir().mkpath(directory);
    QString fileName = release.setupName;
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("SnapOCR-%1-setup.exe").arg(release.version);
    }
    const QString targetPath = QDir(directory).filePath(fileName);

    m_updateDownloading = true;
    auto* progress = new QProgressDialog(tr("正在下载更新 %1…").arg(release.version),
                                         tr("取消"), 0, 100, nullptr);
    progress->setWindowTitle(tr("下载更新"));
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setMinimumDuration(0);
    progress->setValue(0);

    QNetworkRequest request(release.setupUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(120000);
    QNetworkReply* reply = m_updateNetwork->get(request);

    connect(reply, &QNetworkReply::downloadProgress, progress,
            [progress](qint64 received, qint64 total) {
                if (total > 0) {
                    progress->setValue(static_cast<int>(received * 100 / total));
                }
            });
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, progress, targetPath, fileName, release]() {
                progress->close();
                progress->deleteLater();
                m_updateDownloading = false;
                const QByteArray data = reply->readAll();
                const int status =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QString networkError = reply->errorString();
                const bool aborted = reply->error() == QNetworkReply::OperationCanceledError;
                reply->deleteLater();

                if (aborted) {
                    showInfo(tr("已取消下载更新"));
                    return;
                }
                if (reply->error() != QNetworkReply::NoError || status >= 400 || data.isEmpty()) {
                    showError(tr("下载更新失败：%1").arg(networkError));
                    return;
                }
                if (release.setupSize > 0 && data.size() != release.setupSize) {
                    showError(tr("下载文件大小不符（期望 %1 字节，实际 %2 字节），已丢弃")
                                  .arg(release.setupSize)
                                  .arg(data.size()));
                    return;
                }
                QFile file(targetPath);
                if (!file.open(QIODevice::WriteOnly)) {
                    showError(tr("无法写入更新文件：%1").arg(targetPath));
                    return;
                }
                file.write(data);
                file.close();
                qInfo("update downloaded: %s (%lld bytes)", qUtf8Printable(fileName),
                      static_cast<long long>(data.size()));

                const auto answer = QMessageBox::question(
                    nullptr, tr("安装更新"),
                    tr("更新已下载完成。安装过程会关闭 SnapOCR，安装完成后自动重启。\n是否立即安装？"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                if (answer != QMessageBox::Yes) {
                    showInfo(tr("更新已下载，可稍后手动安装：%1").arg(targetPath));
                    return;
                }
                installAndRestart(targetPath);
            });
}

void Application::installAndRestart(const QString& setupPath)
{
    const QString installDirectory = QCoreApplication::applicationDirPath();
    const QString restartScript =
        QDir(QDir::tempPath()).filePath(QStringLiteral("SnapOCR-update-restart.bat"));
    QFile script(restartScript);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        showError(tr("无法创建更新脚本：%1").arg(restartScript));
        return;
    }
    const QString content =
        QStringLiteral("@echo off\r\n"
                       "timeout /t 3 /nobreak >nul\r\n"
                       "\"%1\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS "
                       "/DIR=\"%2\"\r\n"
                       "start \"\" \"%2\\SnapOCR.exe\" --tray\r\n"
                       "del \"%~f0\"\r\n")
            .arg(QDir::toNativeSeparators(setupPath), QDir::toNativeSeparators(installDirectory));
    script.write(content.toUtf8());
    script.close();

    showInfo(tr("正在安装更新，安装完成后会自动重启"));
    const bool started = QProcess::startDetached(QStringLiteral("cmd.exe"),
                                                 {QStringLiteral("/c"), restartScript});
    if (!started) {
        showError(tr("无法启动安装程序，请手动运行：%1").arg(setupPath));
        return;
    }
    QTimer::singleShot(300, qApp, &QCoreApplication::quit);
}

void Application::showInfo(const QString& message)
{
    if (m_tray != nullptr && m_tray->isAvailable()) {
        m_tray->showMessage(QStringLiteral("SnapOCR"), message);
    }
    if (m_mainWindow != nullptr) {
        m_mainWindow->setStatusText(message);
    }
}

void Application::showError(const QString& message)
{
    if (m_tray != nullptr && m_tray->isAvailable()) {
        m_tray->showMessage(QStringLiteral("SnapOCR"), message, true);
    }
    qWarning("%s", qUtf8Printable(message));
    if (m_mainWindow != nullptr) {
        m_mainWindow->setStatusText(message);
    }
}

int Application::runUiPreview(const QString& outputDirectory)
{
    QTextStream out(stdout);
    out << "SnapOCR ui preview\n";

    QString directory = outputDirectory.trimmed();
    if (directory.isEmpty()) {
        out << "FAIL: 缺少输出目录（用法：SnapOCR.exe --ui-preview <目录>）\n";
        out.flush();
        return EXIT_FAILURE;
    }
    if (!QDir().mkpath(directory)) {
        out << "FAIL: 无法创建目录：" << directory << '\n';
        out.flush();
        return EXIT_FAILURE;
    }

    const auto save = [&out, &directory](QWidget* widget, const QString& name, int width,
                                         int height) {
        if (widget == nullptr) {
            return;
        }
        widget->resize(width, height);
        widget->show();
        QApplication::processEvents();
        const QPixmap pixmap = widget->grab();
        const QString path = QDir(directory).filePath(name + QStringLiteral(".png"));
        if (pixmap.save(path, "PNG")) {
            out << "  saved " << name << ".png (" << pixmap.width() << 'x' << pixmap.height()
                << ")\n";
        } else {
            out << "  WARN failed to save " << name << '\n';
        }
        widget->close();
    };

    // 造几张合成截图，供历史窗/贴图/OCR 结果窗使用
    QImage sample(960, 600, QImage::Format_ARGB32);
    sample.fill(QColor(0xF4, 0xF7, 0xFB));
    {
        QPainter painter(&sample);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QFont font(QStringLiteral("Microsoft YaHei"));
        font.setPixelSize(30);
        painter.setFont(font);
        painter.setPen(QColor(0x1F, 0x23, 0x28));
        painter.drawText(QRect(40, 40, 880, 60), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("SnapOCR UI 预览"));
        font.setPixelSize(22);
        painter.setFont(font);
        painter.setPen(QColor(0x5A, 0x62, 0x6B));
        for (int line = 0; line < 6; ++line) {
            painter.drawText(QRect(40, 140 + line * 56, 880, 48),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QStringLiteral("第 %1 行示例文本 · SnapOCR preview line %2")
                                 .arg(line + 1)
                                 .arg(line + 1));
        }
    }

    // ---- 浅色主题 ----
    ui::applyTheme(QStringLiteral("light"));
    save(new ui::MainWindow, QStringLiteral("main-light"), 560, 920);

    {
        ui::OcrResultDialog dialog;
        ocr::OcrResult result;
        result.ok = true;
        result.languageTag = QStringLiteral("zh-Hans-CN");
        result.fullText = QStringLiteral("SnapOCR 离线取字示例。\n第二行文本用于预览。");
        result.cleanText = result.fullText;
        result.elapsedMs = 128;
        ocr::OcrLine line;
        line.text = QStringLiteral("SnapOCR 离线取字示例。");
        result.lines.append(line);
        line.text = QStringLiteral("第二行文本用于预览。");
        result.lines.append(line);
        dialog.setResult(result);
        save(&dialog, QStringLiteral("ocr-result"), 720, 520);
    }

    {
        ui::PinWindow pin(sample);
        save(&pin, QStringLiteral("pin"), 480, 340);
    }
    save(new ui::ScrollingBar, QStringLiteral("scrolling-bar"), 380, 96);

    {
        // 历史窗：用临时目录造 3 条记录
        const QString tempHistory =
            QDir(QDir::tempPath()).filePath(QStringLiteral("snapocr-uipreview-history"));
        QDir(tempHistory).removeRecursively();
        QDir().mkpath(tempHistory);
        history::HistoryStore store;
        store.setDirectory(tempHistory);
        for (int index = 0; index < 3; ++index) {
            QImage image = sample.copy(0, 0, 960, 600);
            QPainter marker(&image);
            marker.fillRect(0, 0, 960, 8, QColor(0x2F, 0x6F, 0xEB));
            marker.end();
            store.add(image);
        }
        ui::HistoryDialog dialog(&store);
        save(&dialog, QStringLiteral("history"), 780, 560);
        QDir(tempHistory).removeRecursively();
    }

    // ---- 深色主题 ----
    ui::applyTheme(QStringLiteral("dark"));
    save(new ui::MainWindow, QStringLiteral("main-dark"), 560, 920);

    // ---- 英文界面 ----
    ui::applyTheme(QStringLiteral("light"));
    i18n::installUiTranslator(qobject_cast<QApplication*>(QApplication::instance()),
                              QStringLiteral("en-US"));
    save(new ui::MainWindow, QStringLiteral("main-english"), 560, 920);
    i18n::installUiTranslator(qobject_cast<QApplication*>(QApplication::instance()),
                              QStringLiteral("zh-CN"));

    out << "PASS\n";
    out.flush();
    return EXIT_SUCCESS;
}

int Application::runSelfTestAll()
{
    QTextStream out(stdout);
    out << "SnapOCR selftest-all\n";
    out << "====================\n";
    out.flush();

    struct StageResult
    {
        QString name;
        int code = EXIT_FAILURE;
    };
    QVector<StageResult> stages;

    const auto runStage = [&out, &stages](const QString& name, const std::function<int()>& fn) {
        out << "\n--- " << name << " ---\n";
        out.flush();
        const int code = fn();
        stages.append({name, code});
        out << (code == EXIT_SUCCESS ? "[OK] " : "[FAIL] ") << name << "\n";
        out.flush();
    };

    runStage(QStringLiteral("smoke"),
             [this]() { return runSmokeTest(); });
    runStage(QStringLiteral("capture"),
             [this]() { return runCaptureSelfTest(); });
    runStage(QStringLiteral("stability(20 rounds)"),
             [this]() { return runStabilitySelfTest(); });
    runStage(QStringLiteral("perf"),
             [this]() { return runPerfSelfTest(); });

    int failed = 0;
    out << "\n====================\n";
    for (const StageResult& stage : stages) {
        out << (stage.code == EXIT_SUCCESS ? "PASS  " : "FAIL  ") << stage.name << '\n';
        if (stage.code != EXIT_SUCCESS) {
            ++failed;
        }
    }
    out << "SUMMARY: " << (stages.size() - failed) << '/' << stages.size() << " passed\n";
    out << (failed == 0 ? "PASS\n" : "FAIL\n");
    out.flush();
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int Application::runSmokeTest()
{
    QTextStream out(stdout);
    out << "SnapOCR smoke test\n";
    out << "  version : " << snapocr::version::kVersion << '\n';

    const QString platformName = QGuiApplication::platformName();
    out << "  platform: " << platformName << '\n';
    if (platformName.isEmpty()) {
        out << "FAIL: Qt 平台插件未加载\n";
        out.flush();
        return EXIT_FAILURE;
    }

    const ui::MainWindow window;
    if (window.windowTitle().isEmpty()) {
        out << "FAIL: 主窗口初始化失败\n";
        out.flush();
        return EXIT_FAILURE;
    }
    out << "  window  : " << window.windowTitle() << '\n';
    out << "PASS\n";
    out.flush();
    return EXIT_SUCCESS;
}

int Application::runStabilitySelfTest()
{
    QTextStream out(stdout);
    out << "SnapOCR stability self test\n";

    constexpr int kIterations = 20;
    platform::win32::ProcessMetrics first;
    platform::win32::ProcessMetrics last;
    platform::win32::ProcessMetrics current;

    // 预热三轮：首次捕获/绘制/控制台接入会一次性分配句柄与字体缓存（实测 +4~+12），
    // 不计入泄漏判定，否则打包版首跑会被误判为“疑似泄漏”。
    for (int warmup = 0; warmup < 3; ++warmup) {
        capture::CaptureResult warmupCaptured;
        QString warmupError;
        if (!platform::win32::ScreenCapture::captureAll(&warmupCaptured, &warmupError)) {
            out << "FAIL: 预热捕获失败：" << warmupError << '\n';
            out.flush();
            return EXIT_FAILURE;
        }
        if (!warmupCaptured.screens.isEmpty()) {
            const QRect geometry = warmupCaptured.screens.first().monitor.physicalGeometry;
            const QRect region = QRect(0, 0, qMin(640, geometry.width()),
                                       qMin(480, geometry.height()));
            const QImage regionImage = capture::cropSelection(warmupCaptured, region);
            annotate::AnnotationDocument document;
            document.setBaseImage(regionImage);
            annotate::AnnotationItem rectangle;
            rectangle.kind = annotate::AnnotationKind::Rectangle;
            rectangle.points = {QPointF(20, 20), QPointF(200, 160)};
            document.addItem(rectangle);
            const QImage rendered = document.render();
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            rendered.save(&buffer, "PNG");
        }
    }

    for (int iteration = 0; iteration < kIterations; ++iteration) {
        capture::CaptureResult captured;
        QString error;
        if (!platform::win32::ScreenCapture::captureAll(&captured, &error)) {
            out << "FAIL: 第 " << iteration + 1 << " 轮捕获失败：" << error << '\n';
            out.flush();
            return EXIT_FAILURE;
        }
        // 模拟一次完整处理链：裁剪 + 标注渲染 + 图像编码
        if (!captured.screens.isEmpty()) {
            const QRect geometry = captured.screens.first().monitor.physicalGeometry;
            const QRect region(geometry.center() - QPoint(200, 120), QSize(400, 240));
            QImage regionImage;
            QString regionError;
            if (!platform::win32::ScreenCapture::captureRegion(region, &regionImage, &regionError)) {
                out << "FAIL: 第 " << iteration + 1 << " 轮区域捕获失败：" << regionError << '\n';
                out.flush();
                return EXIT_FAILURE;
            }

            annotate::AnnotationDocument document;
            document.setBaseImage(regionImage);
            annotate::AnnotationItem rectangle;
            rectangle.kind = annotate::AnnotationKind::Rectangle;
            rectangle.points = {QPointF(20, 20), QPointF(200, 160)};
            document.addItem(rectangle);
            annotate::AnnotationItem blur;
            blur.kind = annotate::AnnotationKind::Blur;
            blur.style.width = 6;
            blur.points = {QPointF(30, 200), QPointF(300, 200)};
            document.addItem(blur);
            const QImage rendered = document.render();
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            rendered.save(&buffer, "PNG");
        }

        platform::win32::sampleProcessMetrics(&current);
        if (iteration == 0) {
            first = current;
        }
        if (iteration == kIterations / 2) {
            out << "  mid     : " << platform::win32::describeMetrics(current) << '\n';
        }
    }
    platform::win32::sampleProcessMetrics(&last);

    out << "  start   : " << platform::win32::describeMetrics(first) << '\n';
    out << "  end     : " << platform::win32::describeMetrics(last) << '\n';

    const qint64 handleDelta = qint64(last.handleCount) - qint64(first.handleCount);
    const qint64 gdiDelta = qint64(last.gdiObjects) - qint64(first.gdiObjects);
    const qint64 userDelta = qint64(last.userObjects) - qint64(first.userObjects);
    const qint64 workingSetDeltaMb =
        (qint64(last.workingSetBytes) - qint64(first.workingSetBytes)) / (1024 * 1024);
    out << "  delta   : handles=" << handleDelta << " gdi=" << gdiDelta
        << " user=" << userDelta << " ws=" << workingSetDeltaMb << "MB\n";

    // 阈值：GDI 是截图类程序最稀缺的资源，要求零增长；句柄/USER 允许运行时抖动
    // （控制台、字体、COM 缓存等，实测 +4~+12），工作集增长不超过 96MB。
    const bool ok = gdiDelta <= 0 && handleDelta <= 16 && userDelta <= 10
                    && workingSetDeltaMb <= 96;
    if (ok) {
        out << "PASS\n";
        out.flush();
        return EXIT_SUCCESS;
    }
    out << "FAIL: 疑似资源泄漏（超过阈值）\n";
    out.flush();
    return EXIT_FAILURE;
}

QString Application::buildDoctorReport()
{

    QString report;
    QTextStream out(&report);
    out << "SnapOCR doctor\n";

    // ---- 版本与系统 ----
    out << "  version      : " << QString::fromUtf8(version::kVersion) << " ("
        << QSysInfo::currentCpuArchitecture() << ")\n";
    out << "  os           : " << QSysInfo::prettyProductName() << " / kernel "
        << QSysInfo::kernelVersion() << "\n";
    out << "  qt           : " << QString::fromLatin1(qVersion()) << '\n';

    // ---- DPI 感知级别 ----
    {
        QString awareness = QStringLiteral("unknown");
        if (const HMODULE user32 = ::GetModuleHandleW(L"user32.dll")) {
            using GetAwarenessFn = DPI_AWARENESS (*)(DPI_AWARENESS_CONTEXT);
            const auto getAwareness = reinterpret_cast<GetAwarenessFn>(
                reinterpret_cast<void*>(::GetProcAddress(user32, "GetAwarenessFromDpiAwarenessContext")));
            if (getAwareness != nullptr) {
                const DPI_AWARENESS_CONTEXT context = ::GetThreadDpiAwarenessContext();
                using AreEqualFn = BOOL (*)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);
                const auto areEqual = reinterpret_cast<AreEqualFn>(reinterpret_cast<void*>(
                    ::GetProcAddress(user32, "AreDpiAwarenessContextsEqual")));
                const bool perMonitorV2 =
                    areEqual != nullptr
                    && areEqual(context, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
                switch (getAwareness(context)) {
                case DPI_AWARENESS_UNAWARE:
                    awareness = QStringLiteral("unaware");
                    break;
                case DPI_AWARENESS_SYSTEM_AWARE:
                    awareness = QStringLiteral("system");
                    break;
                case DPI_AWARENESS_PER_MONITOR_AWARE:
                    awareness = perMonitorV2 ? QStringLiteral("per-monitor-v2")
                                             : QStringLiteral("per-monitor");
                    break;
                default:
                    awareness = QStringLiteral("unknown");
                    break;
                }
            }
        }
        out << "  dpiAwareness : " << awareness << '\n';
    }

    // ---- 显示器 ----
    const QVector<capture::MonitorInfo> monitors = capture::MonitorLayout::enumerate();
    out << "  qtScreens    : " << QGuiApplication::screens().size() << '\n';
    for (const QScreen* screen : QGuiApplication::screens()) {
        out << "    - " << screen->name() << " geom=" << screen->geometry().x() << ','
            << screen->geometry().y() << ' ' << screen->geometry().width() << 'x'
            << screen->geometry().height() << " dpr=" << screen->devicePixelRatio() << '\n';
    }
    out << "  monitors     : " << monitors.size() << '\n';
    for (int index = 0; index < monitors.size(); ++index) {
        const capture::MonitorInfo& monitor = monitors.at(index);
        out << "    [" << index << "] " << monitor.displayName
            << (monitor.primary ? " (primary)" : "") << "  physical="
            << monitor.physicalGeometry.x() << ',' << monitor.physicalGeometry.y() << ' '
            << monitor.physicalGeometry.width() << 'x' << monitor.physicalGeometry.height()
            << "  logical=" << monitor.logicalGeometry.width() << 'x'
            << monitor.logicalGeometry.height() << "  dpr=" << monitor.devicePixelRatio
            << "  rotation=" << monitor.rotationDegrees << "\n";
    }

    // ---- 设置（直接读配置，不启动托盘等副作用） ----
    common::SettingsStore settingsStore;
    QString settingsWarning;
    const common::AppSettings settings = settingsStore.load(&settingsWarning);
    out << "  config       : " << settingsStore.filePath()
        << (common::SettingsStore::portableModeRequested() ? "  (portable)" : "") << '\n';
    if (!settingsWarning.isEmpty()) {
        out << "  configWarning: " << settingsWarning << '\n';
    }
    out << "  settings     : theme=" << settings.theme
        << " language=" << (settings.language.isEmpty() ? QStringLiteral("system")
                                                       : settings.language)
        << " captureHotkey=" << settings.captureHotkey
        << " ocrHotkey=" << settings.ocrHotkey << " logLevel=" << settings.logLevel << '\n';
    out << "  saveDir      : "
        << (settings.saveDirectory.isEmpty() ? common::SettingsStore::defaultSaveDirectory()
                                             : settings.saveDirectory)
        << " format=" << settings.saveFormat << '\n';
    out << "  logs         : " << common::defaultLogDirectory() << '\n';
    out << "  history      : " << history::HistoryStore::defaultHistoryDirectory() << '\n';

    // ---- OCR 语言 ----
    {
        const ocr::WindowsOcrEngine engine;
        const QVector<ocr::OcrLanguage> languages = engine.availableLanguages();
        out << "  ocrLanguages : " << languages.size();
        for (int index = 0; index < languages.size() && index < 6; ++index) {
            out << (index == 0 ? " -> " : "; ") << languages.at(index).tag << ' '
                << languages.at(index).displayName;
        }
        out << '\n';
    }

    // ---- VC++ 运行库（干净机器最常见的启动失败原因） ----
    {
        QStringList found;
        for (const QString& dll :
             {QStringLiteral("vcruntime140.dll"), QStringLiteral("vcruntime140_1.dll"),
              QStringLiteral("msvcp140.dll")}) {
            if (!QFileInfo::exists(QStringLiteral("C:/Windows/System32/") + dll)) {
                found.append(dll + QStringLiteral("=MISSING"));
            }
        }
        out << "  vcruntime    : " << (found.isEmpty() ? QStringLiteral("present") : found.join(", "))
            << '\n';
    }

    // ---- 桌面会话 / 剪贴板 ----
    {
        // 锁屏判定：读当前输入桌面名字——交互会话是 Default，锁屏时是 Winlogon。
        QString desktopName;
        if (const HDESK inputDesktop =
                ::OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP)) {
            wchar_t buffer[256] = {};
            DWORD needed = 0;
            if (::GetUserObjectInformationW(inputDesktop, UOI_NAME, buffer, sizeof(buffer),
                                            &needed)
                != FALSE) {
                desktopName = QString::fromWCharArray(buffer);
            }
            ::CloseDesktop(inputDesktop);
        }
        // 桌面名在锁屏时可能仍报 Default，最可靠的信号是 LogonUI.exe 是否存在
        // （它只在锁屏安全桌面活动期间运行）。剪贴板能否打开作为辅助信号。
        bool logonUiRunning = false;
        if (const HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W entry = {};
            entry.dwSize = sizeof(entry);
            if (::Process32FirstW(snapshot, &entry) != FALSE) {
                do {
                    if (QString::fromWCharArray(entry.szExeFile)
                            .compare(QStringLiteral("LogonUI.exe"), Qt::CaseInsensitive)
                        == 0) {
                        logonUiRunning = true;
                        break;
                    }
                } while (::Process32NextW(snapshot, &entry) != FALSE);
            }
            ::CloseHandle(snapshot);
        }
        out << "  session      : "
            << (logonUiRunning ? QStringLiteral("locked (LogonUI active)")
                               : QStringLiteral("interactive") + " (desktop=" + desktopName + ")")
            << '\n';
        const bool clipboardOk = ::OpenClipboard(nullptr) != FALSE;
        if (clipboardOk) {
            ::CloseClipboard();
        }
        out << "  clipboard    : " << (clipboardOk ? QStringLiteral("accessible")
                                                   : QStringLiteral("busy/locked"))
            << '\n';
    }

    // ---- 全局热键占用情况（临时注册后立即释放） ----
    {
        app::HotkeyManager hotkeys;
        QString captureError;
        QString ocrError;
        const bool captureOk = hotkeys.registerHotkey(app::HotkeyManager::Action::Capture,
                                                      settings.captureHotkey, &captureError);
        const bool ocrOk = hotkeys.registerHotkey(app::HotkeyManager::Action::Ocr,
                                                  settings.ocrHotkey, &ocrError);
        out << "  hotkeys      : capture(" << settings.captureHotkey << ")="
            << (captureOk ? QStringLiteral("ok") : captureError) << " ocr("
            << settings.ocrHotkey << ")=" << (ocrOk ? QStringLiteral("ok") : ocrError) << '\n';
        hotkeys.unregisterAll();
    }

    // ---- 单实例 ----
    {
        const QString key = QStringLiteral("Local\\") + QString::fromUtf8(version::kSingleInstanceKey);
        HANDLE mutex = ::CreateMutexW(nullptr, FALSE, reinterpret_cast<LPCWSTR>(key.utf16()));
        const bool alreadyRunning = mutex != nullptr && ::GetLastError() == ERROR_ALREADY_EXISTS;
        if (mutex != nullptr) {
            ::CloseHandle(mutex);
        }
        out << "  singleInst   : "
            << (alreadyRunning ? QStringLiteral("another instance running")
                               : QStringLiteral("idle"))
            << '\n';
    }

    out << "PASS\n";
    out.flush();
    return report;
}

int Application::runDoctor()
{
    QTextStream out(stdout);
    out << buildDoctorReport();
    out.flush();
    return EXIT_SUCCESS;
}

int Application::runPerfSelfTest()
{
    QTextStream out(stdout);
    out << "SnapOCR perf self test\n";

    bool ok = true;

    // 0) 空闲资源（捕获/OCR 之前采样，代表托盘常驻状态）
    platform::win32::ProcessMetrics idleMetrics;
    platform::win32::sampleProcessMetrics(&idleMetrics);
    const qint64 idleWorkingSetMb = qint64(idleMetrics.workingSetBytes) / (1024 * 1024);
    out << "  idle       : " << platform::win32::describeMetrics(idleMetrics) << '\n';
    if (idleWorkingSetMb > 120) {
        out << "  WARN: 空闲工作集超过 120MB\n";
        ok = false;
    }

    // 1) 屏幕捕获：全屏（多屏拼接）耗时应远低于一帧预算
    QElapsedTimer captureTimer;
    captureTimer.start();
    capture::CaptureResult captured;
    QString captureError;
    const bool captureOk =
        platform::win32::ScreenCapture::captureAll(&captured, &captureError);
    const qint64 captureMs = captureTimer.elapsed();
    if (!captureOk) {
        out << "FAIL: 捕获失败：" << captureError << '\n';
        out.flush();
        return EXIT_FAILURE;
    }
    out << "  captureAll : " << captureMs << " ms (" << captured.virtualGeometry.width()
        << "x" << captured.virtualGeometry.height() << ")\n";
    // 目标：全屏捕获 < 150ms（4K 实测约 58ms）
    if (captureMs >= 150) {
        out << "  WARN: 全屏捕获超过 150ms\n";
        ok = false;
    }

    // 2) 区域捕获 + 裁剪（选区确认后的实际路径）
    QElapsedTimer regionTimer;
    regionTimer.start();
    const QRect region(0, 0, qMin(1920, captured.virtualGeometry.width()),
                       qMin(1080, captured.virtualGeometry.height()));
    const QImage cropped = capture::cropSelection(captured, region.translated(captured.virtualGeometry.topLeft()));
    const qint64 regionMs = regionTimer.elapsed();
    out << "  region     : " << cropped.width() << 'x' << cropped.height() << " (" << regionMs
        << " ms)\n";
    if (cropped.isNull() || regionMs >= 150) {
        out << "  WARN: 区域捕获异常\n";
        ok = false;
    }

    // 3) OCR 端到端：合成一张带文字的 1920x1080 图，走完整异步服务链路
    QImage sample(1920, 1080, QImage::Format_ARGB32);
    sample.fill(Qt::white);
    {
        QPainter painter(&sample);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QFont font(QStringLiteral("Microsoft YaHei"));
        font.setPixelSize(34);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(QRect(80, 100, 1760, 200), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("SnapOCR 性能自检：截图、标注与离线取字。"));
        font.setPixelSize(28);
        painter.setFont(font);
        for (int line = 0; line < 8; ++line) {
            painter.drawText(QRect(80, 320 + line * 70, 1760, 60), Qt::AlignLeft | Qt::AlignVCenter,
                             QStringLiteral("第 %1 行测试文本 SnapOCR performance line %2")
                                 .arg(line + 1)
                                 .arg(line + 1));
        }
    }

    ocr::OcrService service;
    service.setEngine(std::make_unique<ocr::WindowsOcrEngine>());
    service.setTimeoutMs(12000);
    ocr::OcrResult ocrResult;
    bool ocrDone = false;
    connect(&service, &ocr::OcrService::finished, this,
            [&ocrResult, &ocrDone](const ocr::OcrResult& result) {
                ocrResult = result;
                ocrDone = true;
            });
    QElapsedTimer ocrTimer;
    ocrTimer.start();
    service.recognizeAsync(sample, QString());
    if (!ocrDone) {
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(&service, &ocr::OcrService::finished, &loop, &QEventLoop::quit);
        timeoutTimer.start(15000);
        if (!ocrDone) {
            loop.exec();
        }
    }
    const qint64 ocrMs = ocrTimer.elapsed();
    if (!ocrDone) {
        out << "FAIL: OCR 在 15s 内没有返回\n";
        out.flush();
        return EXIT_FAILURE;
    }
    if (!ocrResult.ok) {
        out << "  WARN: OCR 失败：" << ocrResult.errorMessage << '\n';
        ok = false;
    }
    out << "  ocr        : " << ocrMs << " ms, lines=" << ocrResult.lines.size()
        << ", chars=" << ocrResult.fullText.size() << "\n";
    // 目标：1920x1080 区域 OCR < 1000ms
    if (ocrMs >= 1000) {
        out << "  WARN: OCR 超过 1000ms\n";
        ok = false;
    }

    // 4) 自检过程结束时的资源（含保留的 4K 图像，仅作参考，不参与判定）
    platform::win32::ProcessMetrics endMetrics;
    platform::win32::sampleProcessMetrics(&endMetrics);
    out << "  after test : " << platform::win32::describeMetrics(endMetrics) << '\n';

    out << (ok ? "PASS\n" : "FAIL\n");
    out.flush();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int Application::runCaptureSelfTest()
{
    QTextStream out(stdout);
    out << "SnapOCR capture self test\n";
    out << "  platform: " << QGuiApplication::platformName() << '\n';

    out << "  qt screens:" << '\n';
    for (const QScreen* screen : QGuiApplication::screens()) {
        out << "    - name=" << screen->name() << " geom=" << screen->geometry().width() << 'x'
            << screen->geometry().height() << " at (" << screen->geometry().x() << ','
            << screen->geometry().y() << ") dpr=" << screen->devicePixelRatio()
            << " logicalDpi=" << screen->logicalDotsPerInch() << '\n';
    }

    QElapsedTimer captureTimer;
    captureTimer.start();
    capture::CaptureService service;
    capture::CaptureResult captured;
    QString error;
    QObject::connect(&service, &capture::CaptureService::captured,
                     [&captured](const capture::CaptureResult& result) { captured = result; });
    QObject::connect(&service, &capture::CaptureService::failed,
                     [&error](const QString& message) { error = message; });
    service.captureAllScreens();

    if (!captured.isValid()) {
        out << "FAIL: " << (error.isEmpty() ? tr("捕获失败") : error) << '\n';
        out.flush();
        return EXIT_FAILURE;
    }

    out << "  screens : " << captured.screens.size() << '\n';
    for (const capture::ScreenFrame& frame : captured.screens) {
        out << "    - " << frame.monitor.id << " phys=" << frame.monitor.physicalGeometry.width()
            << 'x' << frame.monitor.physicalGeometry.height()
            << " dpr=" << frame.monitor.devicePixelRatio << " image=" << frame.image.width() << 'x'
            << frame.image.height() << '\n';
    }
    out << "  virtual : " << captured.virtualGeometry.width() << 'x'
        << captured.virtualGeometry.height() << '\n';
    out << "  timing  : captureAll=" << captureTimer.elapsed() << "ms\n";

    // 区域捕获（滚动长截图用）：从第一块屏中间抓一块，校验尺寸
    {
        const QRect firstGeometry = captured.screens.first().monitor.physicalGeometry;
        const QRect region(firstGeometry.center() - QPoint(60, 40), QSize(120, 80));
        QImage regionImage;
        QString regionError;
        QElapsedTimer regionTimer;
        regionTimer.start();
        if (platform::win32::ScreenCapture::captureRegion(region, &regionImage, &regionError)) {
            out << "  region  : " << regionImage.width() << 'x' << regionImage.height()
                << " (" << regionTimer.elapsed() << "ms)\n";
            if (regionImage.width() != region.width() || regionImage.height() != region.height()) {
                out << "FAIL: 区域捕获尺寸不符\n";
                out.flush();
                return EXIT_FAILURE;
            }
        } else {
            out << "FAIL: 区域捕获失败：" << regionError << '\n';
            out.flush();
            return EXIT_FAILURE;
        }
    }
    out << "PASS\n";
    out.flush();
    return EXIT_SUCCESS;
}

void Application::printVersion() const
{
    printLine(QStringLiteral("%1 %2")
                  .arg(QString::fromUtf8(snapocr::version::kAppName),
                       QString::fromUtf8(snapocr::version::kVersion)));
}

void Application::printHelp() const
{
    printLine(QStringLiteral("%1 %2")
                  .arg(QString::fromUtf8(snapocr::version::kAppName),
                       QString::fromUtf8(snapocr::version::kVersion)));
    printLine(QStringLiteral("用法: SnapOCR.exe [选项]"));
    printLine(QStringLiteral("选项:"));
    printLine(QStringLiteral("  -h, --help            显示帮助"));
    printLine(QStringLiteral("  -v, --version         显示版本"));
    printLine(QStringLiteral("  --tray                启动后仅驻留托盘"));
    printLine(QStringLiteral("  --settings            启动并直接打开设置"));
    printLine(QStringLiteral("  --smoke-test          启动自检（CI / 构建验证用）"));
    printLine(QStringLiteral("  --capture-selftest    屏幕捕获自检（诊断用）"));
    printLine(QStringLiteral("  --stability-selftest  资源稳定性自检（句柄/GDI/内存，跑 20 轮）"));
    printLine(QStringLiteral("  --crash-selftest      崩溃记录自检（主动触发异常，验证 crash 日志）"));
}

}  // namespace snapocr::app
