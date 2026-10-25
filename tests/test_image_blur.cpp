#include <QtTest>

#include <QImage>

#include "annotate/ImageBlur.h"

using namespace snapocr::annotate;

// 高斯模糊（F-ANN-16）：模糊后局部方差下降、尺寸不变、radius<=0 原样返回。
class ImageBlurTest final : public QObject
{
    Q_OBJECT

private slots:
    void zeroRadiusReturnsSameImage();
    void blurKeepsSize();
    void blurReducesLocalVariation();
    void blurredEdgePixelsChangeNearBoundary();

private:
    static QImage makeStripes();
};

QImage ImageBlurTest::makeStripes()
{
    QImage image(60, 60, QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int value = ((x / 2) % 2 == 0) ? 250 : 5;  // 黑白细条纹
            image.setPixelColor(x, y, QColor(value, value, value));
        }
    }
    return image;
}

void ImageBlurTest::zeroRadiusReturnsSameImage()
{
    const QImage source = makeStripes();
    const QImage blurred = gaussianBlur(source, 0);
    QCOMPARE(blurred.size(), source.size());
    QCOMPARE(blurred.pixelColor(10, 10), source.pixelColor(10, 10));
}

void ImageBlurTest::blurKeepsSize()
{
    const QImage blurred = gaussianBlur(makeStripes(), 6);
    QCOMPARE(blurred.size(), QSize(60, 60));
}

void ImageBlurTest::blurReducesLocalVariation()
{
    const QImage source = makeStripes();
    const QImage blurred = gaussianBlur(source, 6);
    // 整行相邻像素差异的平均值：模糊后应显著下降（条纹被抹平）
    auto meanNeighbourDelta = [](const QImage& image) {
        int total = 0;
        int count = 0;
        const int y = image.height() / 2;
        for (int x = 1; x < image.width(); ++x) {
            total += qAbs(qGray(image.pixel(x, y)) - qGray(image.pixel(x - 1, y)));
            ++count;
        }
        return count > 0 ? static_cast<double>(total) / count : 0.0;
    };
    const double sourceDelta = meanNeighbourDelta(source);
    const double blurredDelta = meanNeighbourDelta(blurred);
    QVERIFY2(blurredDelta < sourceDelta * 0.5,
             qPrintable(QStringLiteral("source=%1 blurred=%2").arg(sourceDelta).arg(blurredDelta)));
}

void ImageBlurTest::blurredEdgePixelsChangeNearBoundary()
{
    QImage image(40, 40, QImage::Format_ARGB32);
    image.fill(Qt::white);
    image.setPixelColor(0, 0, QColor(0, 0, 0));
    const QImage blurred = gaussianBlur(image, 4);
    QVERIFY(qGray(blurred.pixel(0, 0)) > 0);  // 黑点被周围白色平均掉
}

QTEST_MAIN(ImageBlurTest)

#include "test_image_blur.moc"
