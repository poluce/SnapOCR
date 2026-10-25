#include "ocr/OcrHints.h"

namespace snapocr::ocr {
namespace {

constexpr int kTinySelectionSide = 12;  // < 12px 的选区基本无法识别

QString smallSelectionHint(const QSize& size)
{
    return QStringLiteral("选区太小（%1 × %2），请框选更大的区域再识别")
        .arg(size.width())
        .arg(size.height());
}

}  // namespace

bool selectionTouchesScreenEdge(const QRect& selectionPhysical,
                                const QRect& virtualDesktopPhysical)
{
    if (selectionPhysical.isEmpty() || virtualDesktopPhysical.isEmpty()) {
        return false;
    }
    constexpr int kTolerance = 1;
    return qAbs(selectionPhysical.left() - virtualDesktopPhysical.left()) <= kTolerance
           || qAbs(selectionPhysical.top() - virtualDesktopPhysical.top()) <= kTolerance
           || qAbs(selectionPhysical.right() - virtualDesktopPhysical.right()) <= kTolerance
           || qAbs(selectionPhysical.bottom() - virtualDesktopPhysical.bottom()) <= kTolerance;
}

QString describeOcrProblem(const OcrResult& result, const QSize& selectionSize,
                           bool touchesScreenEdge)
{
    const bool tiny = selectionSize.isValid()
                      && (selectionSize.width() < kTinySelectionSide
                          || selectionSize.height() < kTinySelectionSide);
    if (tiny) {
        return smallSelectionHint(selectionSize);
    }

    const QString error = result.errorMessage;
    if (error.contains(QStringLiteral("语言"))
        || error.contains(QStringLiteral("language"), Qt::CaseInsensitive)
        || error.contains(QStringLiteral("pack"), Qt::CaseInsensitive)
        || error.contains(QStringLiteral("0x8007"))) {
        return QStringLiteral(
            "缺少 OCR 语言包：设置 → 时间和语言 → 语言和区域 → 添加语言 → "
            "安装「光学字符识别」可选功能，然后重启 SnapOCR 再试");
    }
    if (error.contains(QStringLiteral("超时")) || error.contains(QStringLiteral("timeout"),
                                                                Qt::CaseInsensitive)) {
        return QStringLiteral("识别超时：可缩小选区，或稍后重试");
    }
    if (error.contains(QStringLiteral("初始化")) || error.contains(QStringLiteral("引擎"))) {
        return QStringLiteral("OCR 引擎不可用：%1").arg(error);
    }
    if (!result.ok && !error.isEmpty()) {
        return QStringLiteral("识别失败：%1").arg(error);
    }

    // 识别成功但没有文字：常见原因是纯图片/低对比度，或选区边缘把文字裁掉了。
    QString hint = QStringLiteral("未识别到文字：可能是纯图片或对比度太低");
    if (touchesScreenEdge) {
        hint += QStringLiteral("；选区贴到了屏幕边缘，边缘的文字可能被裁掉");
    }
    hint += QStringLiteral("，可调整选区后点「重新识别」");
    return hint;
}

}  // namespace snapocr::ocr
