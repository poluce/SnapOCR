#pragma once

#include <memory>

#include <QFutureWatcher>
#include <QImage>
#include <QMetaType>
#include <QObject>
#include <QSet>
#include <QString>

#include "ocr/OcrTypes.h"

class QTimer;

namespace snapocr::ocr {

// OCR 异步调度服务：把识别放到 worker 线程执行，提供超时保护，并把结果回投到服务所在线程。
//
// 线程模型：
// - 在拥有事件循环的线程（通常是 UI 主线程）创建与调用；
// - recognizeAsync() 立即返回，结果统一通过 finished 信号发出；
// - 引擎的 recognize() 只在 worker 线程执行，严禁在 UI 线程直接调用引擎。
//
// 超时：
// - 默认 10000ms（QTimer 无法强杀 worker，超时只表示“不再等待”）；
// - 超时后立即发出失败结果并允许发起新请求，迟到的旧结果会被丢弃。
class OcrService : public QObject
{
    Q_OBJECT

public:
    static constexpr int kDefaultTimeoutMs = 10000;

    explicit OcrService(QObject* parent = nullptr);
    ~OcrService() override;

    // 接管引擎所有权。识别进行中调用会等待当前任务收尾后再替换（仅建议装配期调用）。
    void setEngine(std::unique_ptr<IOcrEngine> engine);
    IOcrEngine* engine() const;

    bool isBusy() const;

    // 超时毫秒数；<= 0 表示不启用超时。
    void setTimeoutMs(int timeoutMs);
    int timeoutMs() const;

    // 发起一次异步识别。忙时立即返回“正在识别中”的失败结果（异步发 finished）。
    void recognizeAsync(const QImage& image, const QString& languageTag = QString());

signals:
    void finished(const snapocr::ocr::OcrResult& result);

private:
    void finishWatcher(QFutureWatcher<OcrResult>* watcher);
    void handleTimeout();
    void emitResultAsync(const OcrResult& result);
    void waitForActiveWatchers();

    std::unique_ptr<IOcrEngine> m_engine;
    QSet<QFutureWatcher<OcrResult>*> m_activeWatchers;
    QFutureWatcher<OcrResult>* m_currentWatcher = nullptr;
    QTimer* m_timeoutTimer = nullptr;
    int m_timeoutMs = kDefaultTimeoutMs;
    bool m_busy = false;
    bool m_currentTimedOut = false;
};

}  // namespace snapocr::ocr

Q_DECLARE_METATYPE(snapocr::ocr::OcrResult)
