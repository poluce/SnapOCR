#include <QtTest>

#include <QImage>
#include <QPainter>

#include "annotate/AnnotationDocument.h"

using namespace snapocr::annotate;

namespace {

AnnotationItem makeRect(const QPointF& a, const QPointF& b, const QColor& color = Qt::red,
                        int width = 3)
{
    AnnotationItem item;
    item.kind = AnnotationKind::Rectangle;
    item.style.color = color;
    item.style.width = width;
    item.points = {a, b};
    return item;
}

QImage makeBaseImage(int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::white);
    return image;
}

}  // namespace

class AnnotationDocumentTest final : public QObject
{
    Q_OBJECT

private slots:
    void setsBaseImageAndClearsItems();
    void renderKeepsImageSize();
    void renderDrawsRectanglePixels();
    void hitTestReturnsTopMostItem();
    void removeItemShiftsIndices();
    void nextStepNumberIncrements();
    void mosaicChangesPixels();
    void computeBoundsExpandsByLineWidth();
};

void AnnotationDocumentTest::setsBaseImageAndClearsItems()
{
    AnnotationDocument document;
    document.setBaseImage(makeBaseImage(100, 80));
    document.addItem(makeRect({1, 1}, {10, 10}));
    QCOMPARE(document.items().size(), 1);

    document.setBaseImage(makeBaseImage(10, 10));
    QCOMPARE(document.items().size(), 0);
    QCOMPARE(document.imageSize(), QSize(10, 10));
}

void AnnotationDocumentTest::renderKeepsImageSize()
{
    AnnotationDocument document;
    document.setBaseImage(makeBaseImage(320, 240));
    document.addItem(makeRect({10, 10}, {100, 100}));
    const QImage rendered = document.render();
    QCOMPARE(rendered.size(), QSize(320, 240));
}

void AnnotationDocumentTest::renderDrawsRectanglePixels()
{
    AnnotationDocument document;
    document.setBaseImage(makeBaseImage(100, 100));
    document.addItem(makeRect({10, 10}, {50, 50}, QColor(Qt::red), 4));

    const QImage rendered = document.render();
    // 边框处应出现红色像素，中心应为白色（未填充）。
    QCOMPARE(rendered.pixelColor(10, 30), QColor(Qt::red));
    QCOMPARE(rendered.pixelColor(30, 30), QColor(Qt::white));
}

void AnnotationDocumentTest::hitTestReturnsTopMostItem()
{
    AnnotationDocument document;
    document.setBaseImage(makeBaseImage(200, 200));
    document.addItem(makeRect({10, 10}, {100, 100}));
    document.addItem(makeRect({20, 20}, {60, 60}));

    // 命中重叠区域时返回后添加（上层）的图元。
    QCOMPARE(document.hitTest(QPointF(40, 40)), 1);
    // 只命中第一项。
    QCOMPARE(document.hitTest(QPointF(90, 90)), 0);
    // 空白区域。
    QCOMPARE(document.hitTest(QPointF(150, 150)), -1);
}

void AnnotationDocumentTest::removeItemShiftsIndices()
{
    AnnotationDocument document;
    document.setBaseImage(makeBaseImage(200, 200));
    document.addItem(makeRect({10, 10}, {40, 40}));
    document.addItem(makeRect({60, 60}, {90, 90}));
    document.removeItem(0);
    QCOMPARE(document.items().size(), 1);
    QCOMPARE(document.items().first().points.first(), QPointF(60, 60));
}

void AnnotationDocumentTest::nextStepNumberIncrements()
{
    AnnotationDocument document;
    document.setBaseImage(makeBaseImage(100, 100));
    QCOMPARE(document.nextStepNumber(), 1);

    AnnotationItem step;
    step.kind = AnnotationKind::StepNumber;
    step.stepNumber = 3;
    step.points = {QPointF(20, 20)};
    document.addItem(step);
    QCOMPARE(document.nextStepNumber(), 4);
}

void AnnotationDocumentTest::mosaicChangesPixels()
{
    QImage base(120, 120, QImage::Format_ARGB32);
    for (int y = 0; y < base.height(); ++y) {
        for (int x = 0; x < base.width(); ++x) {
            base.setPixelColor(x, y, (x / 4 + y / 4) % 2 == 0 ? QColor(Qt::black) : QColor(Qt::white));
        }
    }

    AnnotationDocument document;
    document.setBaseImage(base);

    AnnotationItem mosaic;
    mosaic.kind = AnnotationKind::Mosaic;
    mosaic.style.mosaicBlockSize = 12;
    mosaic.points = {QPointF(20, 20), QPointF(80, 80)};
    document.addItem(mosaic);

    const QImage rendered = document.render();
    // 马赛克区域内的像素应被块化（与原始棋盘图案不同）。
    int differing = 0;
    for (int y = 24; y < 76; ++y) {
        for (int x = 24; x < 76; ++x) {
            if (rendered.pixelColor(x, y) != base.pixelColor(x, y)) {
                ++differing;
            }
        }
    }
    QVERIFY2(differing > 100, qPrintable(QStringLiteral("differing=%1").arg(differing)));
}

void AnnotationDocumentTest::computeBoundsExpandsByLineWidth()
{
    AnnotationItem item = makeRect({10, 10}, {50, 40}, Qt::red, 6);
    const QRectF bounds = computeBounds(item);
    QVERIFY(bounds.left() <= 10);
    QVERIFY(bounds.top() <= 10);
    QVERIFY(bounds.right() >= 50);
    QVERIFY(bounds.bottom() >= 40);
}

QTEST_GUILESS_MAIN(AnnotationDocumentTest)

#include "test_annotation_document.moc"
