#include "ocr/OcrTextUtils.h"

#include <algorithm>
#include <cmath>

#include <QChar>
#include <QStringList>

namespace snapocr::ocr {
namespace {

// 判断汉字/日文假名/全角标点等宽字符。用于删除 OCR 在汉字之间插入的多余空格。
bool isCjkChar(QChar ch)
{
    const ushort code = ch.unicode();
    return (code >= 0x3400 && code <= 0x4DBF)      // 扩展 A
           || (code >= 0x4E00 && code <= 0x9FFF)   // 基本区
           || (code >= 0xF900 && code <= 0xFAFF)   // 兼容汉字
           || (code >= 0xFF00 && code <= 0xFFEF)   // 全角形式（含全角字母/标点）
           || (code >= 0x3000 && code <= 0x303F)   // 中日韩标点（。、，等）
           || (code >= 0x2010 && code <= 0x2027);  // 破折号、省略号、引号等通用标点
}

// 是否为 CJK 标点（不是文字）。用于判断跨行拼接时是否需要补空格。
bool isCjkPunctuation(QChar ch)
{
    const ushort code = ch.unicode();
    return (code >= 0x3000 && code <= 0x303F) || (code >= 0x2010 && code <= 0x2027)
           || (code >= 0xFF01 && code <= 0xFF0F) || (code >= 0xFF1A && code <= 0xFF20)
           || (code >= 0xFF3B && code <= 0xFF40) || (code >= 0xFF5B && code <= 0xFF65);
}

bool isAsciiWordChar(QChar ch)
{
    const ushort code = ch.unicode();
    return (code >= 0x30 && code <= 0x39) || (code >= 0x41 && code <= 0x5A)
           || (code >= 0x61 && code <= 0x7A);
}

bool isSpaceChar(QChar ch)
{
    const ushort code = ch.unicode();
    return ch.isSpace() || code == 0x00A0 || code == 0x3000;
}

// 行内空格整理：连续空白压缩为一个半角空格，删除汉字之间的空格；
// 行首行尾空白直接丢弃（配合跨行合并由调用方处理）。
QString stripLineSpaces(const QString& line)
{
    QString result;
    result.reserve(line.size());
    bool pendingSpace = false;
    for (const QChar ch : line)
    {
        if (isSpaceChar(ch))
        {
            pendingSpace = !result.isEmpty();
            continue;
        }
        if (pendingSpace)
        {
            // 只有“汉字 + 汉字”之间的空格才判定为 OCR 伪影；
            // 西文之间、中西文之间的空格保留一个，避免把英文单词粘连。
            if (!(isCjkChar(result.back()) && isCjkChar(ch)))
            {
                result.append(QLatin1Char(' '));
            }
            pendingSpace = false;
        }
        result.append(ch);
    }
    return result;
}

// 行尾是否为句末标点（允许标点后跟收尾引号/括号）。
bool endsWithParagraphTerminator(const QString& text)
{
    static const QString kClosingChars = QStringLiteral("”’」』】）》〉\"'");
    static const QString kTerminators = QStringLiteral("。！？…!?.");
    QString trimmed = text;
    while (!trimmed.isEmpty() && kClosingChars.contains(trimmed.back()))
    {
        trimmed.chop(1);
    }
    return !trimmed.isEmpty() && kTerminators.contains(trimmed.back());
}

// 跨行拼接时两个字符之间是否需要补空格。
bool needsSpaceBetween(QChar previous, QChar next)
{
    if (isAsciiWordChar(previous) && isAsciiWordChar(next))
    {
        return true;
    }
    // 西文单词后紧跟汉字时补空格，避免 “SnapOCR是工具” 这类粘连；中文后接西文则不补。
    if (isAsciiWordChar(previous) && isCjkChar(next) && !isCjkPunctuation(next))
    {
        return true;
    }
    // 西文标点（逗号、右括号等）后紧跟西文单词时补空格。
    return !isCjkChar(previous) && isAsciiWordChar(next);
}

// 单个坐标轴上的切片规划结果（起点 + 统一块长度）。
struct AxisPlan
{
    QVector<int> starts;
    int chunkSize = 0;
};

// 规划单个坐标轴：保证覆盖完整、每块不超过 maxDimension、相邻块重叠 ≥ overlap。
// 推导：块数 count = ceil((length - overlap) / (maxDimension - overlap))；
// 块长取 ceil((length + (count - 1) * overlap) / count)，此时相邻块重叠恰好约 overlap，
// 且块长不超过 maxDimension（由 count 的定义可证）。
AxisPlan planAxis(int length, int maxDimension, int overlap)
{
    AxisPlan plan;
    if (length <= 0)
    {
        return plan;
    }
    if (maxDimension <= 0 || length <= maxDimension)
    {
        plan.starts = {0};
        plan.chunkSize = length;
        return plan;
    }

    const int usable = maxDimension - overlap;
    // overlap 已限制在 maxDimension/2 以内，因此 usable > 0。
    const int count = (length - overlap + usable - 1) / usable;
    plan.chunkSize = (length + (count - 1) * overlap + count - 1) / count;
    const int span = length - plan.chunkSize;
    plan.starts.reserve(count);
    for (int index = 0; index < count; ++index)
    {
        plan.starts.append((index * span) / (count - 1));
    }
    return plan;
}

}  // namespace

QVector<QRect> planSlices(const QSize& imageSize, int maxDimension, int overlapPercent)
{
    if (imageSize.isEmpty())
    {
        return {};
    }

    const int width = imageSize.width();
    const int height = imageSize.height();
    if (maxDimension <= 0 || (width <= maxDimension && height <= maxDimension))
    {
        return {QRect(0, 0, width, height)};
    }

    const int clampedPercent = qBound(0, overlapPercent, 50);
    const int overlap = qBound(0, qRound(maxDimension * clampedPercent / 100.0), maxDimension / 2);

    const AxisPlan xPlan = planAxis(width, maxDimension, overlap);
    const AxisPlan yPlan = planAxis(height, maxDimension, overlap);
    if (xPlan.starts.isEmpty() || yPlan.starts.isEmpty())
    {
        return {};
    }

    QVector<QRect> slices;
    slices.reserve(xPlan.starts.size() * yPlan.starts.size());
    for (const int top : yPlan.starts)
    {
        for (const int left : xPlan.starts)
        {
            const int sliceWidth = qMin(xPlan.chunkSize, width - left);
            const int sliceHeight = qMin(yPlan.chunkSize, height - top);
            slices.append(QRect(left, top, sliceWidth, sliceHeight));
        }
    }
    return slices;
}

QVector<OcrLine> mergeDuplicateLines(const QVector<OcrLine>& lines)
{
    if (lines.isEmpty())
    {
        return {};
    }

    QVector<OcrLine> ordered = lines;
    std::stable_sort(ordered.begin(), ordered.end(), [](const OcrLine& left, const OcrLine& right) {
        if (left.boundingBox.top() != right.boundingBox.top())
        {
            return left.boundingBox.top() < right.boundingBox.top();
        }
        return left.boundingBox.left() < right.boundingBox.left();
    });

    const auto sameRow = [](const QRectF& left, const QRectF& right) {
        const qreal minHeight = qMin(left.height(), right.height());
        const qreal tolerance = qMax<qreal>(4.0, minHeight * 0.5);
        return qAbs(left.center().y() - right.center().y()) <= tolerance;
    };
    const auto horizontalOverlap = [](const QRectF& left, const QRectF& right) {
        const qreal overlapWidth =
            qMin(left.right(), right.right()) - qMax(left.left(), right.left());
        if (overlapWidth <= 0.0)
        {
            return false;
        }
        const qreal minWidth = qMin(left.width(), right.width());
        return minWidth <= 0.0 || overlapWidth >= minWidth * 0.5;
    };

    QVector<OcrLine> merged;
    merged.reserve(ordered.size());
    for (const OcrLine& line : ordered)
    {
        if (line.text.trimmed().isEmpty())
        {
            continue;
        }
        bool duplicate = false;
        for (const OcrLine& kept : merged)
        {
            if (kept.text.simplified() == line.text.simplified()
                && sameRow(kept.boundingBox, line.boundingBox)
                && horizontalOverlap(kept.boundingBox, line.boundingBox))
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            merged.append(line);
        }
    }
    return merged;
}

QString normalizeText(const QString& rawText)
{
    if (rawText.isEmpty())
    {
        return {};
    }

    QString text = rawText;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QStringList paragraphs;
    QString current;
    bool hasCurrent = false;
    const auto flushParagraph = [&paragraphs, &current, &hasCurrent]() {
        if (hasCurrent && !current.isEmpty())
        {
            paragraphs.append(current);
        }
        current.clear();
        hasCurrent = false;
    };

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines)
    {
        const QString line = stripLineSpaces(rawLine);
        if (line.isEmpty())
        {
            flushParagraph();
            continue;
        }
        if (!hasCurrent)
        {
            current = line;
            hasCurrent = true;
            continue;
        }
        if (endsWithParagraphTerminator(current))
        {
            flushParagraph();
            current = line;
            hasCurrent = true;
            continue;
        }
        // 西文行尾连字符：去掉连字符并直接拼接（inter-\nnational -> international）。
        if (current.endsWith(QLatin1Char('-')) && isAsciiWordChar(line.front()))
        {
            current.chop(1);
            current.append(line);
            continue;
        }
        if (needsSpaceBetween(current.back(), line.front()))
        {
            current.append(QLatin1Char(' '));
        }
        current.append(line);
    }
    flushParagraph();

    return paragraphs.join(QLatin1Char('\n'));
}

}  // namespace snapocr::ocr
