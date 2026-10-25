#pragma once

#include <QRect>
#include <QImage>
#include <QObject>
#include <QTimer>
#include <QVector>

namespace snapocr::capture {

// 滚动长截图的抓帧状态机（F-CAP-16）。
//
// 与窗口/屏幕解耦：抓帧动作由外部注入的 FrameProvider 提供，因此可以在单测里
// 用合成帧驱动整个流程（开始 → 抓帧 → 自动结束 → 拼接）。
class ScrollingCaptureController final : public QObject
{
    Q_OBJECT

public:
    // 抓一帧；返回 false 表示抓取失败（error 可给出原因）。
    using FrameProvider = std::function<bool(QImage*, QString*)>;

    explicit ScrollingCaptureController(QObject* parent = nullptr);

    void setFrameProvider(FrameProvider provider);
    void setIntervalMs(int intervalMs);
    int intervalMs() const { return m_intervalMs; }

    // 开始：立即抓第一帧，之后按 interval 抓帧。
    void start(const QRect& physicalRect);
    // 用户点「完成」：停止抓帧并拼接。
    void finish();
    // 用户取消：丢弃所有帧。
    void cancel();

    bool isActive() const { return m_active; }
    int frameCount() const { return m_frames.size(); }
    QRect region() const { return m_region; }

signals:
    void started();
    void progress(int frames, int stitchedHeight);
    void finished(const QImage& stitched);
    void failed(const QString& message);

private slots:
    // 定时器/测试触发的单帧抓取
    void captureFrame();

private:
    void appendFrame(const QImage& frame);

    FrameProvider m_provider;
    QTimer* m_timer = nullptr;
    QRect m_region;
    QVector<QImage> m_frames;
    int m_intervalMs = 500;
    bool m_active = false;
    int m_stallCount = 0;

    // 保护条件：避免无限增长
    static constexpr int kMaxFrames = 120;
    static constexpr int kStallLimit = 3;
};

}  // namespace snapocr::capture
