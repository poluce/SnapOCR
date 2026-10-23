#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <memory>

#include "common/Settings.h"
#include "ocr/OcrTypes.h"

namespace snapocr::ui {
class MainWindow;
class OcrResultDialog;
}

namespace snapocr::overlay {
class OverlayController;
}

namespace snapocr::ocr {
class OcrService;
}

namespace snapocr::app {

class HotkeyManager;
class SingleInstance;
class TrayController;

// 应用控制器：解析命令行、装配托盘/热键/单实例/截图流程，连接输出动作。
class Application final : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    // 处理命令行参数并进入事件循环，返回进程退出码。
    int run(const QStringList& arguments);

private:
    int runSmokeTest();
    int runCaptureSelfTest();
    void printVersion() const;
    void printHelp() const;

    bool initializeServices();
    void applyHotkeys();
    void registerOverlayHotkeys(bool enabled);
    void updateHotkeyLabels();

    void startCapture();
    void startOcrCapture();
    void onCopyRequested(const QImage& image);
    void onSaveRequested(const QImage& image);
    void onCaptureError(const QString& message);

    void runOcr(const QImage& image);
    void onOcrFinished(const snapocr::ocr::OcrResult& result);
    ui::OcrResultDialog* ensureOcrDialog();

    void showMainWindow();
    void showSettings();
    void quitApplication();

    void showInfo(const QString& message);
    void showError(const QString& message);
    void persistSettings();

    std::unique_ptr<common::SettingsStore> m_settingsStore;
    common::AppSettings m_settings;

    HotkeyManager* m_hotkeys = nullptr;
    TrayController* m_tray = nullptr;
    SingleInstance* m_singleInstance = nullptr;
    overlay::OverlayController* m_overlayController = nullptr;
    ocr::OcrService* m_ocrService = nullptr;

    std::unique_ptr<ui::MainWindow> m_mainWindow;
    QPointer<ui::OcrResultDialog> m_ocrDialog;
    QImage m_lastOcrImage;
    bool m_initialized = false;
};

}  // namespace snapocr::app
