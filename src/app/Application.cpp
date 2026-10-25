#include "app/Application.h"

#include <QKeyEvent>

#include <cstdio>
#include <cstdlib>
#include <memory>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCursor>
#include <QLineEdit>
#include <QTextEdit>
#include <QLoggingCategory>
#include <QDebug>
#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include "app/HotkeyManager.h"
#include "app/ImageSaver.h"
#include "app/SingleInstance.h"
#include "app/TrayController.h"
#include "capture/CaptureService.h"
#include "common/Logging.h"
#include "common/Settings.h"
#include "ocr/OcrService.h"
#include "ocr/WindowsOcrEngine.h"
#include "overlay/OverlayController.h"
#include "platform/win32/Clipboard.h"
#include "platform/win32/Startup.h"
#include "platform/win32/WindowActivation.h"
#include "snapocr/Version.h"
#include "history/HistoryStore.h"
#include "ui/HistoryDialog.h"
#include "ui/MainWindow.h"
#include "ui/OcrResultDialog.h"
#include "ui/PinWindow.h"
#include "ui/Theme.h"

namespace snapocr::app {
namespace {

constexpr auto kFlagHelp = "--help";
constexpr auto kFlagVersion = "--version";
constexpr auto kFlagSmokeTest = "--smoke-test";
constexpr auto kFlagCaptureSelfTest = "--capture-selftest";
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

    common::installLogging(m_settings.logLevel.isEmpty() ? common::defaultLogDirectory()
                                                         : common::defaultLogDirectory(),
                           m_settings.logLevel);
    ui::applyTheme(m_settings.theme);

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
    connect(m_tray, &TrayController::settingsRequested, this, &Application::showSettings);
    connect(m_tray, &TrayController::showMainRequested, this, &Application::showMainWindow);
    connect(m_tray, &TrayController::quitRequested, this, &Application::quitApplication);
    m_tray->show();

    m_overlayController = new overlay::OverlayController(this);
    connect(m_overlayController, &overlay::OverlayController::copyRequested, this,
            &Application::onCopyRequested);
    connect(m_overlayController, &overlay::OverlayController::pinRequested, this,
            &Application::pinImage);
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
                    showInfo(QStringLiteral("已复制颜色 %1").arg(hex));
                } else {
                    showError(QStringLiteral("复制颜色失败：%1").arg(error));
                }
            });

    connect(m_overlayController, &overlay::OverlayController::started, this, [this]() {
        registerOverlayHotkeys(true);
    });
    connect(m_overlayController, &overlay::OverlayController::finished, this, [this]() {
        registerOverlayHotkeys(false);
        if (m_mainWindow != nullptr) {
            m_mainWindow->setStatusText(QStringLiteral("就绪"));
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
                      ? QStringLiteral("%1，已临时改用 F1。可在设置中更换截图热键。").arg(captureError)
                      : QStringLiteral("%1，且默认 F1 也不可用。请在设置中更换截图热键。")
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
                      ? QStringLiteral("%1，已临时改用 F4。可在设置中更换取字热键。").arg(ocrError)
                      : QStringLiteral("%1，且默认 F4 也不可用。请在设置中更换取字热键。")
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
        QString err;
        const bool okCancel = m_hotkeys->registerHotkey(
            HotkeyManager::Action::OverlayCancel, QStringLiteral("Esc"), &err);
        QString err2;
        const bool okConfirm = m_hotkeys->registerHotkey(
            HotkeyManager::Action::OverlayConfirm, QStringLiteral("Enter"), &err2);
    } else {
        m_hotkeys->unregisterHotkey(HotkeyManager::Action::OverlayCancel);
        m_hotkeys->unregisterHotkey(HotkeyManager::Action::OverlayConfirm);
    }
}

void Application::startCapture()
{
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
                                QStringLiteral("%1 秒后截图…").arg(delay));
        }
        QTimer::singleShot(delay * 1000, this, [this]() {
            m_delayPending = false;
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
        m_mainWindow->setStatusText(ocrMode ? QStringLiteral("取字：拖拽选择区域")
                                            : QStringLiteral("截图：拖拽选择区域"));
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
                                QStringLiteral("%1 秒后取字…").arg(delay));
        }
        QTimer::singleShot(delay * 1000, this, [this]() {
            m_delayPending = false;
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
    connect(pin, &QObject::destroyed, this, [this, pin]() { m_pins.removeAll(QPointer<ui::PinWindow>(pin)); });

    m_pins.append(QPointer<ui::PinWindow>(pin));
    if (m_mainWindow != nullptr) {
        m_mainWindow->setStatusText(QStringLiteral("已贴图（滚轮缩放，R 旋转，Esc 关闭）"));
    }
}

void Application::onCopyRequested(const QImage& image)
{
    recordHistory(image);
    QString error;
    if (platform::win32::copyImageToClipboard(image, &error)) {
        showInfo(QStringLiteral("已复制到剪贴板（%1 × %2）").arg(image.width()).arg(image.height()));
    } else {
        showError(QStringLiteral("复制失败：%1").arg(error));
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
        showInfo(QStringLiteral("已保存：%1").arg(QDir::toNativeSeparators(savedPath)));
    } else {
        showError(QStringLiteral("保存失败：%1").arg(error));
    }
}

void Application::onCaptureError(const QString& message)
{
    showError(message);
}

void Application::runOcr(const QImage& image)
{
    if (image.isNull()) {
        showError(QStringLiteral("没有可识别的图像"));
        return;
    }

    m_lastOcrImage = image;

    ui::OcrResultDialog* dialog = ensureOcrDialog();
    dialog->setBusy(QStringLiteral("正在识别…（离线本地识别）"));
    dialog->show();
    dialog->raise();
    dialog->activateWindow();

    if (m_ocrService == nullptr) {
        return;
    }
    if (m_ocrService->isBusy()) {
        dialog->setResult(ocr::OcrResult{.ok = false,
                                         .errorMessage = QStringLiteral("上一次识别还没结束，请稍候")});
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
        showInfo(QStringLiteral("识别文本已复制到剪贴板"));
    });
    connect(dialog, &ui::OcrResultDialog::retryRequested, this, [this, dialog]() {
        if (!m_lastOcrImage.isNull()) {
            const QImage image = m_lastOcrImage;
            dialog->setBusy(QStringLiteral("正在重新识别…"));
            m_ocrService->recognizeAsync(image, m_settings.ocrLanguage);
        }
    });
    m_ocrDialog = dialog;
    return dialog;
}

void Application::onOcrFinished(const ocr::OcrResult& result)
{
    ui::OcrResultDialog* dialog = ensureOcrDialog();
    dialog->setResult(result);
    dialog->show();
    dialog->raise();

    if (!result.ok) {
        showError(QStringLiteral("文字识别失败：%1").arg(result.errorMessage));
        return;
    }

    const QString text = result.fullText.trimmed();
    if (text.isEmpty()) {
        showInfo(QStringLiteral("未识别到文字，可重新框选更大区域再试"));
        return;
    }

    if (m_settings.ocrAutoCopy) {
        QApplication::clipboard()->setText(result.fullText);
        showInfo(QStringLiteral("已识别 %1 行文字并复制到剪贴板（%2ms）")
                     .arg(result.lines.size())
                     .arg(result.elapsedMs));
    } else {
        showInfo(QStringLiteral("已识别 %1 行文字（%2ms）")
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
    }

    if (m_settings.theme != previous.theme) {
        ui::applyTheme(m_settings.theme);
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
        m_mainWindow->setStatusText(QStringLiteral("设置已保存"));
    }
}

void Application::persistSettings()
{
    if (m_settingsStore == nullptr) {
        return;
    }
    QString error;
    if (!m_settingsStore->save(m_settings, &error)) {
        showError(QStringLiteral("设置保存失败：%1").arg(error));
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

    capture::CaptureService service;
    capture::CaptureResult captured;
    QString error;
    QObject::connect(&service, &capture::CaptureService::captured,
                     [&captured](const capture::CaptureResult& result) { captured = result; });
    QObject::connect(&service, &capture::CaptureService::failed,
                     [&error](const QString& message) { error = message; });
    service.captureAllScreens();

    if (!captured.isValid()) {
        out << "FAIL: " << (error.isEmpty() ? QStringLiteral("捕获失败") : error) << '\n';
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
}

}  // namespace snapocr::app
