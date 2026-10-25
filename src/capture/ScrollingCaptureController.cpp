#include "capture/ScrollingCaptureController.h"

#include <QCryptographicHash>

#include "capture/ScrollingStitch.h"

namespace snapocr::capture {
namespace {

QString digest(const QImage& image)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    hash.addData(QByteArray(reinterpret_cast<const char*>(argb.constBits()),
                            static_cast<qsizetype>(argb.sizeInBytes())));
    return QString::fromLatin1(hash.result().toHex());
}

}  // namespace

ScrollingCaptureController::ScrollingCaptureController(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(m_intervalMs);
    connect(m_timer, &QTimer::timeout, this, &ScrollingCaptureController::captureFrame);
}

void ScrollingCaptureController::setFrameProvider(FrameProvider provider)
{
    m_provider = std::move(provider);
}

void ScrollingCaptureController::setIntervalMs(int intervalMs)
{
    m_intervalMs = qBound(100, intervalMs, 5000);
    m_timer->setInterval(m_intervalMs);
}

void ScrollingCaptureController::start(const QRect& physicalRect)
{
    if (m_active || physicalRect.isEmpty()) {
        return;
    }
    m_region = physicalRect;
    m_frames.clear();
    m_stallCount = 0;
    m_active = true;
    emit started();

    captureFrame();  // 第一帧立刻抓，之后由定时器驱动
    if (m_active) {
        m_timer->start(m_intervalMs);
    }
}

void ScrollingCaptureController::captureFrame()
{
    if (!m_active || !m_provider) {
        return;
    }
    QImage frame;
    QString error;
    if (!m_provider(&frame, &error)) {
        m_timer->stop();
        m_active = false;
        emit failed(error.isEmpty() ? QStringLiteral("抓取屏幕失败") : error);
        return;
    }
    appendFrame(frame);
}

void ScrollingCaptureController::appendFrame(const QImage& frame)
{
    if (frame.isNull()) {
        return;
    }
    if (!m_frames.isEmpty() && digest(m_frames.last()) == digest(frame)) {
        // 页面没滚动：连续多帧没变化就认为到底了，自动结束
        if (++m_stallCount >= kStallLimit) {
            finish();
        }
        return;
    }
    m_stallCount = 0;
    m_frames.append(frame);
    emit progress(m_frames.size(), m_frames.size() * frame.height());

    if (m_frames.size() >= kMaxFrames) {
        finish();
    }
}

void ScrollingCaptureController::finish()
{
    if (!m_active) {
        return;
    }
    m_timer->stop();
    m_active = false;

    const StitchResult result = stitchFrames(m_frames, 40);
    if (!result.ok) {
        emit failed(result.error);
        return;
    }
    if (result.image.isNull()) {
        emit failed(QStringLiteral("没有可用的截图内容"));
        return;
    }
    emit finished(result.image);
}

void ScrollingCaptureController::cancel()
{
    if (!m_active) {
        return;
    }
    m_timer->stop();
    m_active = false;
    m_frames.clear();
}

}  // namespace snapocr::capture
