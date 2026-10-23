#include "ocr/OcrService.h"

#include <exception>
#include <utility>

#include <QElapsedTimer>
#include <QFuture>
#include <QMetaObject>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include "ocr/OcrLogging.h"

namespace snapocr::ocr {
namespace {

OcrResult makeFailure(const QString& message, qint64 elapsedMs = 0)
{
    OcrResult result;
    result.ok = false;
    result.errorMessage = message;
    result.elapsedMs = elapsedMs;
    return result;
}

}  // namespace

OcrService::OcrService(QObject* parent) : QObject(parent)
{
    // 让 finished 信号可以跨线程排队投递（QSignalSpy/UI 也可能按值保存结果）。
    qRegisterMetaType<snapocr::ocr::OcrResult>("snapocr::ocr::OcrResult");

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() { handleTimeout(); });
}

OcrService::~OcrService()
{
    // 等待仍在运行的后台任务结束，避免 worker 线程访问已析构的引擎/服务。
    waitForActiveWatchers();
}

void OcrService::setEngine(std::unique_ptr<IOcrEngine> engine)
{
    if (m_busy || !m_activeWatchers.isEmpty())
    {
        qCWarning(lcOcr) << "识别进行中替换 OCR 引擎，将先等待当前任务结束。";
    }
    waitForActiveWatchers();
    m_engine = std::move(engine);
}

IOcrEngine* OcrService::engine() const
{
    return m_engine.get();
}

bool OcrService::isBusy() const
{
    return m_busy;
}

void OcrService::setTimeoutMs(int timeoutMs)
{
    m_timeoutMs = timeoutMs;
}

int OcrService::timeoutMs() const
{
    return m_timeoutMs;
}

void OcrService::recognizeAsync(const QImage& image, const QString& languageTag)
{
    if (m_busy)
    {
        emitResultAsync(makeFailure(QStringLiteral("正在识别中，请等待当前识别完成后再试。")));
        return;
    }
    if (!m_engine)
    {
        emitResultAsync(makeFailure(QStringLiteral("OCR 引擎未初始化，无法识别。")));
        return;
    }
    if (image.isNull())
    {
        emitResultAsync(makeFailure(QStringLiteral("待识别图像为空，无法识别。")));
        return;
    }

    IOcrEngine* const engine = m_engine.get();
    m_busy = true;
    m_currentTimedOut = false;

    auto* watcher = new QFutureWatcher<OcrResult>(this);
    m_activeWatchers.insert(watcher);
    m_currentWatcher = watcher;
    connect(watcher,
            &QFutureWatcher<OcrResult>::finished,
            this,
            [this, watcher]() { finishWatcher(watcher); });

    if (m_timeoutMs > 0)
    {
        m_timeoutTimer->start(m_timeoutMs);
    }

    // 图像按值捕获：QImage 隐式共享，worker 只读，不会与 UI 线程竞争。
    QFuture<OcrResult> future = QtConcurrent::run([engine, image, languageTag]() -> OcrResult {
        QElapsedTimer elapsedTimer;
        elapsedTimer.start();

        OcrResult result;
        try
        {
            result = engine->recognize(image, languageTag);
        }
        catch (const std::exception& error)
        {
            // 引擎契约要求自己捕获异常，这里兜底，避免异常穿透 QFuture。
            result = OcrResult{};
            result.ok = false;
            result.errorMessage = QStringLiteral("OCR 识别失败：内部异常，请重试。");
            qCWarning(lcOcr) << "OCR 引擎抛出未捕获异常：" << error.what();
        }
        catch (...)
        {
            result = OcrResult{};
            result.ok = false;
            result.errorMessage = QStringLiteral("OCR 识别失败：未知异常，请重试。");
            qCWarning(lcOcr) << "OCR 引擎抛出未知异常。";
        }

        // 以服务端到端耗时为准（包含引擎内部耗时），便于 UI 展示。
        result.elapsedMs = elapsedTimer.elapsed();
        return result;
    });
    watcher->setFuture(future);
}

void OcrService::finishWatcher(QFutureWatcher<OcrResult>* watcher)
{
    m_activeWatchers.remove(watcher);
    if (watcher != m_currentWatcher)
    {
        // 超时后已被放弃的旧任务：只回收 watcher，不再重复发结果。
        watcher->deleteLater();
        return;
    }

    m_timeoutTimer->stop();
    m_currentWatcher = nullptr;
    m_busy = false;
    const bool timedOut = m_currentTimedOut;
    m_currentTimedOut = false;

    const OcrResult result = watcher->result();
    watcher->deleteLater();

    if (!timedOut)
    {
        emit finished(result);
    }
}

void OcrService::handleTimeout()
{
    if (!m_busy || m_currentWatcher == nullptr)
    {
        return;
    }

    m_currentTimedOut = true;
    m_busy = false;
    // 不再等待该任务：置空当前 watcher，迟到结果由 finishWatcher() 丢弃。
    m_currentWatcher = nullptr;
    qCWarning(lcOcr) << "OCR 识别超时，已返回失败结果，超时阈值：" << m_timeoutMs << "ms";

    emit finished(makeFailure(
        QStringLiteral("OCR 识别超时（超过 %1 毫秒），请重试或缩小识别区域。").arg(m_timeoutMs),
        m_timeoutMs));
}

void OcrService::emitResultAsync(const OcrResult& result)
{
    // 立即失败也走事件循环，保证调用方总在异步上下文中收到 finished，避免重入。
    QMetaObject::invokeMethod(
        this, [this, result]() { emit finished(result); }, Qt::QueuedConnection);
}

void OcrService::waitForActiveWatchers()
{
    m_timeoutTimer->stop();
    const auto watchers = m_activeWatchers;
    for (QFutureWatcher<OcrResult>* watcher : watchers)
    {
        watcher->waitForFinished();
        delete watcher;
    }
    m_activeWatchers.clear();
    m_currentWatcher = nullptr;
    m_busy = false;
    m_currentTimedOut = false;
}

}  // namespace snapocr::ocr
