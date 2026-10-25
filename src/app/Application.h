#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QRect>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QStringList>

#include <memory>

#include "common/Settings.h"

#include <QVector>
#include "ocr/OcrTypes.h"

namespace snapocr::ui {
class MainWindow;
class PinWindow;
class OcrResultDialog;
}


namespace snapocr::annotate {
class AnnotationDocument;
}

namespace snapocr::editor {
class EditorWindow;
}

namespace snapocr::overlay {
class OverlayController;
}

namespace snapocr::ocr {
class OcrService;
}

namespace snapocr::capture {
class ScrollingCaptureController;
}

namespace snapocr::ui {
class ScrollingBar;
}

namespace snapocr::history {
class HistoryStore;
}

namespace snapocr::ui {
class HistoryDialog;
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
    int runStabilitySelfTest();
    // 性能自检：捕获耗时 / OCR 端到端耗时 / 进程资源（阶段三 §8.2 验收）
    int runPerfSelfTest();
    void printVersion() const;
    void printHelp() const;

    bool initializeServices();
    void applyHotkeys();
    void registerOverlayHotkeys(bool enabled);
    void forwardOverlayKey(int key);
    void updateHotkeyLabels();

    void startCapture();
    void beginCaptureNow(bool ocrMode);
    void startOcrCapture();
    void onCopyRequested(const QImage& image);
    void pinImage(const QImage& image);
    // F-ANN-19：把任意图片放进标注编辑器（贴图「在编辑器中打开」用）
    void openEditorForImage(const QImage& image);
    void showHistory();
    // 托盘「贴图最近一张」：取历史里最新一张贴到屏幕
    void pinLatestFromHistory();
    // 滚动长截图（F-CAP-16）
    void startScrollingCapture(const QRect& physicalRect);
    void finishScrollingCapture();
    void cancelScrollingCapture();
    void recordHistory(const QImage& image);
    void onSaveRequested(const QImage& image);
    void onCaptureError(const QString& message);

    void runOcr(const QImage& image);
    void onOcrFinished(const snapocr::ocr::OcrResult& result);
    ui::OcrResultDialog* ensureOcrDialog();

    void showMainWindow();
    void showSettings();
    void applySettings(const snapocr::common::AppSettings& incoming);
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
    // 贴图窗口列表（Qt::WA_DeleteOnClose，析构自动移除）
    QVector<QPointer<ui::PinWindow>> m_pins;
    history::HistoryStore* m_history = nullptr;
    QPointer<ui::HistoryDialog> m_historyDialog;
    QPointer<editor::EditorWindow> m_pinEditor;
    std::unique_ptr<annotate::AnnotationDocument> m_pinEditorDocument;
    capture::ScrollingCaptureController* m_scrolling = nullptr;
    QRect m_scrollingRegion;  // 本次长截图的物理区域
    QPointer<ui::ScrollingBar> m_scrollingBar;
    QImage m_lastOcrImage;
    QSize m_lastOcrSelectionSize;
    bool m_lastOcrTouchesEdge = false;
    bool m_initialized = false;

    // 延时截图：倒计时期间忽略重复触发，到点后按记录的模式开始捕获。
    bool m_delayPending = false;
    // 热键→遮罩耗时：startCapture 开始时启动，覆盖层显示时记录
    QElapsedTimer m_captureLatencyTimer;
    bool m_delayOcrMode = false;
};

}  // namespace snapocr::app
