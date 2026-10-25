#include <QtTest>

#include <QImage>
#include <QVector>

#include "capture/ScrollingStitch.h"

using namespace snapocr::capture;

// 滚动长截图拼接（F-CAP-16）：
// 用“已知长图 + 固定步长切片”的方式构造帧，验证重叠搜索与拼接结果。
class ScrollingStitchTest final : public QObject
{
    Q_OBJECT

private slots:
    void findsKnownOffset();
    void rejectsUnrelatedFrames();
    void stitchesFramesIntoOriginalHeight();
    void skipsDuplicateFrames();
    void widthMismatchFails();
    void emptyFramesFail();

private:
    static QImage makeLongImage(int width, int height);
    static QVector<QImage> slice(const QImage& longImage, int frameHeight, int step);
};

QImage ScrollingStitchTest::makeLongImage(int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32);
    // 用线性同余生成器给每行一个“伪随机”颜色：避免周期性导致错误位移也能吻合
    quint32 state = 0x1234567u;
    for (int y = 0; y < height; ++y) {
        state = state * 1664525u + 1013904223u;
        const QRgb row = qRgb((state >> 8) & 0xFF, (state >> 16) & 0xFF, (state >> 24) & 0xFF);
        for (int x = 0; x < width; ++x) {
            image.setPixel(x, y, row);
        }
    }
    return image;
}

QVector<QImage> ScrollingStitchTest::slice(const QImage& longImage, int frameHeight, int step)
{
    QVector<QImage> frames;
    for (int top = 0; top + frameHeight <= longImage.height(); top += step) {
        frames.append(longImage.copy(0, top, longImage.width(), frameHeight));
    }
    return frames;
}

void ScrollingStitchTest::findsKnownOffset()
{
    const QImage longImage = makeLongImage(120, 1600);
    const QVector<QImage> frames = slice(longImage, 500, 200);
    QVERIFY(frames.size() >= 3);

    int offset = -1;
    double score = -1;
    QVERIFY(findVerticalOverlap(frames[0], frames[1], 100, 400, &offset, &score));
    QCOMPARE(offset, 200);
    QVERIFY(score >= 0.0 && score < 12.0);
}

void ScrollingStitchTest::rejectsUnrelatedFrames()
{
    const QImage a = makeLongImage(80, 300);
    QImage b(80, 300, QImage::Format_ARGB32);
    for (int y = 0; y < b.height(); ++y) {
        for (int x = 0; x < b.width(); ++x) {
            b.setPixel(x, y, qRgb((y * 37) % 255, 255 - (y % 255), (x * 11) % 255));
        }
    }
    int offset = -1;
    QVERIFY(!findVerticalOverlap(a, b, 100, 200, &offset, nullptr));
}

void ScrollingStitchTest::stitchesFramesIntoOriginalHeight()
{
    const QImage longImage = makeLongImage(100, 2000);
    const QVector<QImage> frames = slice(longImage, 600, 250);
    QVERIFY(frames.size() >= 4);

    const StitchResult result = stitchFrames(frames, 100);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.image.width(), 100);
    // 最后一帧的底部正好是长图底部：拼完应与原长图等高
    // 向下滚动 250px，每帧只新增 250 行；首帧贡献整帧
    const int expectedHeight = 600 + (frames.size() - 1) * 250;
    QCOMPARE(result.image.height(), expectedHeight);
    QCOMPARE(result.offsets.size(), frames.size() - 1);

    // 抽查若干行像素与原长图一致
    for (int y : {0, 100, 700, expectedHeight - 1}) {
        QCOMPARE(result.image.pixelColor(10, y),
                 longImage.pixelColor(10, qMin(y, longImage.height() - 1)));
    }
}

void ScrollingStitchTest::skipsDuplicateFrames()
{
    const QImage longImage = makeLongImage(90, 900);
    const QImage frame = longImage.copy(0, 0, 90, 400);
    const QVector<QImage> frames{frame, frame, frame};
    const StitchResult result = stitchFrames(frames, 100);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.image.height(), 400);  // 页面没滚动，不应变长
}

void ScrollingStitchTest::widthMismatchFails()
{
    const QImage a = makeLongImage(80, 400);
    const QImage b = makeLongImage(120, 400);
    const StitchResult result = stitchFrames({a, b}, 100);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("尺寸不一致")));
}

void ScrollingStitchTest::emptyFramesFail()
{
    const StitchResult result = stitchFrames({}, 100);
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
}

QTEST_GUILESS_MAIN(ScrollingStitchTest)

#include "test_scrolling_stitch.moc"
