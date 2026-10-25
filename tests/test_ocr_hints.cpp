#include <QtTest>

#include "ocr/OcrHints.h"

using namespace snapocr::ocr;

// 识别失败/无结果提示（F-OCR-09）：错误信息要能翻译成用户可执行的下一步动作。
class OcrHintsTest final : public QObject
{
    Q_OBJECT

private slots:
    void tinySelectionHint();
    void missingLanguageHint();
    void timeoutHint();
    void emptyResultMentionsTrimmedEdge();
    void emptyResultWithoutEdge();
    void genericFailure();
    void edgeDetection();

private:
    static OcrResult okResult();
    static OcrResult failedResult(const QString& message);
};

OcrResult OcrHintsTest::okResult()
{
    OcrResult result;
    result.ok = true;
    result.fullText = QStringLiteral("hello");
    return result;
}

OcrResult OcrHintsTest::failedResult(const QString& message)
{
    OcrResult result;
    result.ok = false;
    result.errorMessage = message;
    return result;
}

void OcrHintsTest::tinySelectionHint()
{
    const QString hint = describeOcrProblem(okResult(), QSize(8, 60), false);
    QVERIFY(hint.contains(QStringLiteral("选区太小")));
    QVERIFY(hint.contains(QStringLiteral("8 × 60")));
}

void OcrHintsTest::missingLanguageHint()
{
    const QString hint =
        describeOcrProblem(failedResult(QStringLiteral("未安装中文 OCR 语言包")), QSize(200, 100),
                           false);
    QVERIFY(hint.contains(QStringLiteral("语言包")));
    QVERIFY(hint.contains(QStringLiteral("光学字符识别")));
}

void OcrHintsTest::timeoutHint()
{
    const QString hint =
        describeOcrProblem(failedResult(QStringLiteral("识别超时（10000ms）")), QSize(200, 100), false);
    QVERIFY(hint.contains(QStringLiteral("超时")));
    QVERIFY(hint.contains(QStringLiteral("缩小选区")));
}

void OcrHintsTest::emptyResultMentionsTrimmedEdge()
{
    const QString hint = describeOcrProblem(okResult(), QSize(300, 200), true);
    QVERIFY(hint.contains(QStringLiteral("未识别到文字")));
    QVERIFY(hint.contains(QStringLiteral("屏幕边缘")));
    QVERIFY(hint.contains(QStringLiteral("重新识别")));
}

void OcrHintsTest::emptyResultWithoutEdge()
{
    const QString hint = describeOcrProblem(okResult(), QSize(300, 200), false);
    QVERIFY(hint.contains(QStringLiteral("未识别到文字")));
    QVERIFY(!hint.contains(QStringLiteral("屏幕边缘")));
}

void OcrHintsTest::genericFailure()
{
    const QString hint =
        describeOcrProblem(failedResult(QStringLiteral("设备未就绪")), QSize(200, 100), false);
    QVERIFY(hint.contains(QStringLiteral("设备未就绪")));
}

void OcrHintsTest::edgeDetection()
{
    const QRect desktop(0, 0, 3840, 2160);
    QVERIFY(selectionTouchesScreenEdge(QRect(0, 100, 300, 200), desktop));
    QVERIFY(selectionTouchesScreenEdge(QRect(3540, 100, 300, 200), desktop));
    QVERIFY(selectionTouchesScreenEdge(QRect(100, 0, 300, 200), desktop));
    QVERIFY(selectionTouchesScreenEdge(QRect(100, 1960, 300, 200), desktop));
    QVERIFY(!selectionTouchesScreenEdge(QRect(500, 500, 300, 200), desktop));
    QVERIFY(!selectionTouchesScreenEdge(QRect(), desktop));
}

QTEST_GUILESS_MAIN(OcrHintsTest)

#include "test_ocr_hints.moc"
