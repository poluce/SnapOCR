#include "capture/ScrollingStitch.h"

#include <QColor>
#include <QCryptographicHash>
#include <QPainter>

#include <cmath>

namespace snapocr::capture {
namespace {

// 行采样步长与列采样步长：只比对一部分像素，保证拼接速度。
constexpr int kRowStep = 4;
constexpr int kColumnStep = 7;

// 平均绝对灰度差阈值：超过认为“不是同一段内容”。
constexpr double kMaxAcceptableScore = 12.0;

QString imageDigest(const QImage& image)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    hash.addData(QByteArray(reinterpret_cast<const char*>(argb.constBits()),
                            static_cast<qsizetype>(argb.sizeInBytes())));
    return QString::fromLatin1(hash.result().toHex());
}

double rowDifference(const QRgb* previousRow, const QRgb* nextRow, int width)
{
    double total = 0.0;
    int samples = 0;
    for (int x = 0; x < width; x += kColumnStep) {
        const int a = qGray(previousRow[x]);
        const int b = qGray(nextRow[x]);
        total += std::abs(a - b);
        ++samples;
    }
    return samples > 0 ? total / samples : 0.0;
}

}  // namespace

bool findVerticalOverlap(const QImage& previous, const QImage& next, int minOverlap, int maxOffset,
                         int* offset, double* score)
{
    if (previous.isNull() || next.isNull() || previous.width() != next.width()) {
        return false;
    }
    const int width = previous.width();
    const int height = previous.height();
    const int nextHeight = next.height();
    if (height < minOverlap || nextHeight < minOverlap) {
        return false;
    }

    // 位移 offset 表示“next 的内容相对 previous 向上移动了多少行”。
    // 重叠行数 = min(height - offset, nextHeight)。
    const int upperBound = qMin(maxOffset, height - minOverlap);
    if (upperBound < 1) {
        return false;
    }

    double bestScore = 1e9;
    int bestOffset = -1;
    for (int candidate = 1; candidate <= upperBound; ++candidate) {
        const int overlapRows = qMin(height - candidate, nextHeight);
        if (overlapRows < minOverlap) {
            break;
        }
        const int step = qMax(kRowStep, overlapRows / 48);  // 大区域时自动加大步长
        double total = 0.0;
        int samples = 0;
        for (int y = 0; y < overlapRows; y += step) {
            const auto* previousRow = reinterpret_cast<const QRgb*>(previous.constScanLine(y + candidate));
            const auto* nextRow = reinterpret_cast<const QRgb*>(next.constScanLine(y));
            total += rowDifference(previousRow, nextRow, width);
            ++samples;
        }
        if (samples == 0) {
            continue;
        }
        const double candidateScore = total / samples;
        if (candidateScore < bestScore) {
            bestScore = candidateScore;
            bestOffset = candidate;
        }
    }

    if (bestOffset < 0 || bestScore > kMaxAcceptableScore) {
        return false;
    }
    if (offset != nullptr) {
        *offset = bestOffset;
    }
    if (score != nullptr) {
        *score = bestScore;
    }
    return true;
}

StitchResult stitchFrames(const QVector<QImage>& frames, int minOverlap)
{
    StitchResult result;
    if (frames.isEmpty()) {
        result.error = QStringLiteral("没有抓到任何画面");
        return result;
    }
    if (frames.first().isNull()) {
        result.error = QStringLiteral("画面为空");
        return result;
    }

    QImage stitched = frames.first().convertToFormat(QImage::Format_ARGB32);
    int appendedRows = 0;

    for (int i = 1; i < frames.size(); ++i) {
        const QImage& next = frames[i];
        if (next.isNull() || next.width() != stitched.width()) {
            result.error = QStringLiteral("第 %1 帧尺寸不一致，无法拼接").arg(i + 1);
            return result;
        }

        // 页面完全没动：直接跳过（不算失败）
        if (i > 0 && !frames[i - 1].isNull() && frames[i - 1].size() == next.size()
            && imageDigest(frames[i - 1]) == imageDigest(next)) {
            continue;
        }

        int offset = 0;
        double score = 0.0;
        // previous 用“当前长图的末尾部分”与 next 比较，避免长图越长搜索越慢
        const QImage tail = stitched.copy(0, qMax(0, stitched.height() - next.height()),
                                         stitched.width(),
                                         qMin(next.height(), stitched.height()));
        const bool found = findVerticalOverlap(tail, next, minOverlap, tail.height() - minOverlap,
                                               &offset, &score);
        if (!found) {
            result.error = QStringLiteral(
                "第 %1 帧与上一帧没有重叠：滚动太快或页面已到底，请放慢滚动速度后重试")
                               .arg(i + 1);
            return result;
        }

        // 语义：next[y] == previous[y + offset]
        // 重叠行数 = 帧高 - offset；新增内容只有 offset 行（向下滚动多少就露出多少）
        const int overlapRows = next.height() - offset;
        const int newRows = offset;
        if (newRows <= 0 || overlapRows < 0) {
            continue;
        }

        QImage grown(stitched.width(), stitched.height() + newRows, QImage::Format_ARGB32);
        QPainter painter(&grown);
        painter.drawImage(0, 0, stitched);
        painter.drawImage(0, stitched.height(), next, 0, overlapRows, next.width(), newRows);
        painter.end();
        stitched = grown;
        appendedRows += newRows;
        result.offsets.append(offset);

        if (stitched.height() > 20000) {
            result.error = QStringLiteral("长截图超过 20000 像素，已停止继续拼接");
            break;
        }
    }

    result.ok = true;
    result.image = stitched;
    if (appendedRows == 0 && frames.size() > 1) {
        // 所有帧都一样：返回原图仍然有意义（用户可能误触）
        result.error.clear();
    }
    return result;
}

}  // namespace snapocr::capture
