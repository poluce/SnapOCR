#include <QtTest>

#include <QRect>
#include <QSize>
#include <QVector>

#include "ocr/OcrTextUtils.h"

using snapocr::ocr::mergeDuplicateLines;
using snapocr::ocr::normalizeText;
using snapocr::ocr::OcrLine;
using snapocr::ocr::planSlices;

namespace {

OcrLine makeLine(const QString& text, const QRectF& boundingBox)
{
    OcrLine line;
    line.text = text;
    line.boundingBox = boundingBox;
    return line;
}

int rightExclusive(const QRect& rect)
{
    return rect.x() + rect.width();
}

int bottomExclusive(const QRect& rect)
{
    return rect.y() + rect.height();
}

}  // namespace

class OcrTextUtilsTest final : public QObject
{
    Q_OBJECT

private slots:
    void returnsSingleSliceForSmallImage();
    void returnsSingleSliceAtExactLimit();
    void splitsWideImageWithExpectedOverlap();
    void tilesOversizedImageIntoGrid();
    void handlesInvalidAndUnlimitedInput();
    void handlesZeroOverlap();

    void mergesWrappedChineseLines();
    void keepsParagraphBoundaries();
    void removesExtraSpacesInChinese();
    void joinsLatinLinesWithSpaces();
    void joinsChineseAndEnglish();
    void handlesCrlfAndEmptyInput();
    void removesHyphenAtLineBreak();
    void keepsTerminatorBeforeClosingQuote();

    void removesDuplicatesFromOverlappingSlices();
    void keepsSameTextAtDifferentRowsOrColumns();
    void sortsLinesByReadingOrder();
    void dropsEmptyTextLines();
    void dedupesWhitespaceVariants();
};

void OcrTextUtilsTest::returnsSingleSliceForSmallImage()
{
    const QVector<QRect> slices = planSlices(QSize(800, 600), 2000);
    QCOMPARE(slices.size(), 1);
    QCOMPARE(slices.first(), QRect(0, 0, 800, 600));
}

void OcrTextUtilsTest::returnsSingleSliceAtExactLimit()
{
    const QVector<QRect> slices = planSlices(QSize(2000, 2000), 2000);
    QCOMPARE(slices.size(), 1);
    QCOMPARE(slices.first(), QRect(0, 0, 2000, 2000));
}

void OcrTextUtilsTest::splitsWideImageWithExpectedOverlap()
{
    // 3000x1000、上限 2000、默认 5% 重叠：切成左右两块，每块 1550 宽，重叠 100 像素。
    const QVector<QRect> slices = planSlices(QSize(3000, 1000), 2000);
    QCOMPARE(slices.size(), 2);
    QCOMPARE(slices.at(0), QRect(0, 0, 1550, 1000));
    QCOMPARE(slices.at(1), QRect(1450, 0, 1550, 1000));
    QCOMPARE(rightExclusive(slices.at(0)) - slices.at(1).x(), 100);
    QCOMPARE(rightExclusive(slices.at(1)), 3000);
}

void OcrTextUtilsTest::tilesOversizedImageIntoGrid()
{
    // 5000x4000、上限 2000：两个方向都需要切，形成 3x3 网格。
    const QSize imageSize(5000, 4000);
    const QVector<QRect> slices = planSlices(imageSize, 2000);
    QCOMPARE(slices.size(), 9);

    const QVector<int> expectedX = {0, 1633, 3266};
    const QVector<int> expectedY = {0, 1300, 2600};
    QCOMPARE(slices.at(0).x(), expectedX.at(0));
    QCOMPARE(slices.at(2).x(), expectedX.at(2));
    QCOMPARE(slices.at(6).y(), expectedY.at(2));

    for (const QRect& slice : slices)
    {
        QVERIFY(slice.width() <= 2000);
        QVERIFY(slice.height() <= 2000);
        QVERIFY(slice.x() >= 0);
        QVERIFY(slice.y() >= 0);
        QVERIFY(rightExclusive(slice) <= imageSize.width());
        QVERIFY(bottomExclusive(slice) <= imageSize.height());
    }

    // 按行检查完整覆盖与相邻重叠（默认 5% 的上限尺寸 = 100 像素）。
    for (int row = 0; row < 3; ++row)
    {
        const int rowBase = row * 3;
        QCOMPARE(slices.at(rowBase).x(), 0);
        QCOMPARE(rightExclusive(slices.at(rowBase + 2)), imageSize.width());
        for (int column = 1; column < 3; ++column)
        {
            const QRect& previous = slices.at(rowBase + column - 1);
            const QRect& current = slices.at(rowBase + column);
            QVERIFY(rightExclusive(previous) - current.x() >= 100);
            QCOMPARE(current.y(), previous.y());
        }
    }
    for (int column = 0; column < 3; ++column)
    {
        QVERIFY(slices.at(column).y() == 0);
        QCOMPARE(bottomExclusive(slices.at(6 + column)), imageSize.height());
    }
}

void OcrTextUtilsTest::handlesInvalidAndUnlimitedInput()
{
    QVERIFY(planSlices(QSize(), 2000).isEmpty());
    QVERIFY(planSlices(QSize(0, 500), 2000).isEmpty());

    const QVector<QRect> unlimited = planSlices(QSize(100, 50), 0);
    QCOMPARE(unlimited.size(), 1);
    QCOMPARE(unlimited.first(), QRect(0, 0, 100, 50));

    const QVector<QRect> negativeLimit = planSlices(QSize(100, 50), -5);
    QCOMPARE(negativeLimit.size(), 1);
    QCOMPARE(negativeLimit.first(), QRect(0, 0, 100, 50));
}

void OcrTextUtilsTest::handlesZeroOverlap()
{
    const QVector<QRect> slices = planSlices(QSize(4000, 10), 1000, 0);
    QCOMPARE(slices.size(), 4);
    for (int index = 0; index < slices.size(); ++index)
    {
        QCOMPARE(slices.at(index), QRect(index * 1000, 0, 1000, 10));
    }
}

void OcrTextUtilsTest::mergesWrappedChineseLines()
{
    // 中文排版换行不应产生空格；句末标点作为段落结束。
    const QString normalized = normalizeText(QStringLiteral("这是一个\n被换行拆开的\n段落。"));
    QCOMPARE(normalized, QStringLiteral("这是一个被换行拆开的段落。"));
}

void OcrTextUtilsTest::keepsParagraphBoundaries()
{
    const QString normalized = normalizeText(
        QStringLiteral("第一段内容。\n第二段第一行\n第二段第二行\n\n第三段。"));
    QCOMPARE(normalized,
             QStringLiteral("第一段内容。\n第二段第一行第二段第二行\n第三段。"));
}

void OcrTextUtilsTest::removesExtraSpacesInChinese()
{
    QCOMPARE(normalizeText(QStringLiteral("你 好 ， 世 界")), QStringLiteral("你好，世界"));
    QCOMPARE(normalizeText(QStringLiteral("  多 余 空 格  ")), QStringLiteral("多余空格"));
}

void OcrTextUtilsTest::joinsLatinLinesWithSpaces()
{
    const QString normalized =
        normalizeText(QStringLiteral("The quick brown\nfox jumps over\nthe lazy dog."));
    QCOMPARE(normalized, QStringLiteral("The quick brown fox jumps over the lazy dog."));
}

void OcrTextUtilsTest::joinsChineseAndEnglish()
{
    // 西文单词后接汉字补一个空格；汉字之间不补。
    QCOMPARE(normalizeText(QStringLiteral("SnapOCR\n是 个 工 具")),
             QStringLiteral("SnapOCR 是个工具"));
}

void OcrTextUtilsTest::handlesCrlfAndEmptyInput()
{
    QCOMPARE(normalizeText(QString()), QString());
    QCOMPARE(normalizeText(QStringLiteral("\r\n\r\n")), QString());
    QCOMPARE(normalizeText(QStringLiteral("   ")), QString());
    QCOMPARE(normalizeText(QStringLiteral("第一行  \r\n第二行\t \r\n")),
             QStringLiteral("第一行第二行"));
}

void OcrTextUtilsTest::removesHyphenAtLineBreak()
{
    QCOMPARE(normalizeText(QStringLiteral("inter-\nnational standard")),
             QStringLiteral("international standard"));
}

void OcrTextUtilsTest::keepsTerminatorBeforeClosingQuote()
{
    QCOMPARE(normalizeText(QStringLiteral("“你好。”\n下一段内容")),
             QStringLiteral("“你好。”\n下一段内容"));
}

void OcrTextUtilsTest::removesDuplicatesFromOverlappingSlices()
{
    const QVector<OcrLine> lines = {
        makeLine(QStringLiteral("你好世界"), QRectF(100, 100, 200, 30)),
        makeLine(QStringLiteral("你好世界"), QRectF(102, 101, 198, 29)),
        makeLine(QStringLiteral("第二行"), QRectF(100, 150, 100, 30)),
    };
    const QVector<OcrLine> merged = mergeDuplicateLines(lines);
    QCOMPARE(merged.size(), 2);
    QCOMPARE(merged.at(0).text, QStringLiteral("你好世界"));
    QCOMPARE(merged.at(1).text, QStringLiteral("第二行"));
}

void OcrTextUtilsTest::keepsSameTextAtDifferentRowsOrColumns()
{
    const QVector<OcrLine> lines = {
        makeLine(QStringLiteral("确定"), QRectF(10, 10, 60, 24)),
        makeLine(QStringLiteral("确定"), QRectF(10, 100, 60, 24)),
        makeLine(QStringLiteral("是"), QRectF(10, 10, 20, 24)),
        makeLine(QStringLiteral("是"), QRectF(400, 10, 20, 24)),
    };
    QCOMPARE(mergeDuplicateLines(lines).size(), 4);
}

void OcrTextUtilsTest::sortsLinesByReadingOrder()
{
    const QVector<OcrLine> lines = {
        makeLine(QStringLiteral("第三行"), QRectF(10, 100, 80, 30)),
        makeLine(QStringLiteral("第一行"), QRectF(10, 10, 80, 30)),
        makeLine(QStringLiteral("第二行"), QRectF(10, 55, 80, 30)),
    };
    const QVector<OcrLine> merged = mergeDuplicateLines(lines);
    QCOMPARE(merged.size(), 3);
    QCOMPARE(merged.at(0).text, QStringLiteral("第一行"));
    QCOMPARE(merged.at(1).text, QStringLiteral("第二行"));
    QCOMPARE(merged.at(2).text, QStringLiteral("第三行"));
}

void OcrTextUtilsTest::dropsEmptyTextLines()
{
    const QVector<OcrLine> lines = {
        makeLine(QStringLiteral("   "), QRectF(0, 0, 10, 10)),
        makeLine(QStringLiteral("保留"), QRectF(10, 10, 10, 10)),
    };
    const QVector<OcrLine> merged = mergeDuplicateLines(lines);
    QCOMPARE(merged.size(), 1);
    QCOMPARE(merged.first().text, QStringLiteral("保留"));
}

void OcrTextUtilsTest::dedupesWhitespaceVariants()
{
    // 同一行在不同切片里可能因空格差异产生不同文本，去重时按 simplified() 比较。
    const QVector<OcrLine> lines = {
        makeLine(QStringLiteral("你好 世界"), QRectF(100, 100, 200, 30)),
        makeLine(QStringLiteral("你好  世界"), QRectF(101, 100, 199, 30)),
    };
    QCOMPARE(mergeDuplicateLines(lines).size(), 1);
}

QTEST_GUILESS_MAIN(OcrTextUtilsTest)

#include "test_ocr_text_utils.moc"
