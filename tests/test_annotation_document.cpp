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
    void scaleItemScalesRectangle();
    void scaleItemScalesFreehandPath();
    void scaleItemScalesTextFontSize();
    void layerOrderMovesItemToTop();
    void moveItemUpSwapsOrder();
    void blurStrokeChangesPixels();
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

void AnnotationDocumentTest::scaleItemScalesRectangle()
{
    AnnotationDocument document;
    document.setBaseImage(QImage(200, 200, QImage::Format_ARGB32));
    AnnotationItem item;
    item.kind = AnnotationKind::Rectangle;
    item.points = {QPointF(20, 20), QPointF(80, 60)};
    document.addItem(item);

    QVERIFY(document.scaleItem(0, QRectF(100, 100, 120, 80)));
    const AnnotationItem scaled = document.items().at(0);
    QCOMPARE(scaled.points.first().toPoint(), QPoint(100, 100));
    QCOMPARE(scaled.points.last().toPoint(), QPoint(220, 180));
}

void AnnotationDocumentTest::scaleItemScalesFreehandPath()
{
    AnnotationDocument document;
    document.setBaseImage(QImage(200, 200, QImage::Format_ARGB32));
    AnnotationItem item;
    item.kind = AnnotationKind::Freehand;
    item.points = {QPointF(10, 10), QPointF(20, 20), QPointF(30, 10)};
    item.style.width = 2;
    document.addItem(item);

    const QRectF before = computeBounds(document.items().at(0));
    QVERIFY2(document.scaleItem(0, QRectF(0, 0, before.width() * 2, before.height() * 2)),
             qPrintable(QStringLiteral("before=%1,%2 %3x%4")
                            .arg(before.left()).arg(before.top()).arg(before.width()).arg(before.height())));
    const QRectF after = computeBounds(document.items().at(0));
    // computeBounds 含线宽 padding，缩放比例按内容宽度判定（允许 10% 误差）
    QVERIFY(after.width() > before.width() * 1.8);
    QVERIFY(after.width() < before.width() * 2.2);
}

void AnnotationDocumentTest::scaleItemScalesTextFontSize()
{
    AnnotationDocument document;
    document.setBaseImage(QImage(200, 200, QImage::Format_ARGB32));
    AnnotationItem item;
    item.kind = AnnotationKind::Text;
    item.text = QStringLiteral("文字");
    item.style.fontSize = 20;
    item.points = {QPointF(50, 50)};
    document.addItem(item);

    const QRectF before = computeBounds(document.items().at(0));
    const QRectF target(before.topLeft(), before.size() * 2.0);
    QVERIFY(document.scaleItem(0, target));
    QCOMPARE(document.items().at(0).style.fontSize, 40);
}

void AnnotationDocumentTest::layerOrderMovesItemToTop()
{
    AnnotationDocument document;
    document.setBaseImage(QImage(80, 80, QImage::Format_ARGB32));
    for (int i = 0; i < 3; ++i) {
        AnnotationItem item;
        item.kind = AnnotationKind::Rectangle;
        item.points = {QPointF(i, i), QPointF(i + 10, i + 10)};
        document.addItem(item);
    }
    const QPointF marker = document.items().at(0).points.first();
    QVERIFY(document.moveItemToTop(0));
    QCOMPARE(document.items().size(), 3);
    QCOMPARE(document.items().last().points.first(), marker);
    QVERIFY(document.moveItemToBottom(2));
    QCOMPARE(document.items().first().points.first(), marker);
}

void AnnotationDocumentTest::moveItemUpSwapsOrder()
{
    AnnotationDocument document;
    document.setBaseImage(QImage(80, 80, QImage::Format_ARGB32));
    for (int i = 0; i < 2; ++i) {
        AnnotationItem item;
        item.kind = AnnotationKind::Ellipse;
        item.points = {QPointF(i, i), QPointF(i + 5, i + 5)};
        document.addItem(item);
    }
    const QPointF first = document.items().at(0).points.first();
    QVERIFY(document.moveItemUp(0));
    QCOMPARE(document.items().at(1).points.first(), first);
    QVERIFY(!document.moveItemUp(1));
    QVERIFY(document.moveItemDown(1));
    QCOMPARE(document.items().at(0).points.first(), first);
}

void AnnotationDocumentTest::blurStrokeChangesPixels()
{
    AnnotationDocument document;
    QImage base(120, 80, QImage::Format_ARGB32);
    for (int y = 0; y < base.height(); ++y) {
        for (int x = 0; x < base.width(); ++x) {
            base.setPixelColor(x, y, ((x / 2) % 2 == 0) ? Qt::white : Qt::black);
        }
    }
    document.setBaseImage(base);

    AnnotationItem item;
    item.kind = AnnotationKind::Blur;
    item.style.width = 4;
    item.style.blurRadius = 6;
    item.points = {QPointF(20, 40), QPointF(100, 40)};
    document.addItem(item);

    const QImage rendered = document.render();
    QCOMPARE(rendered.size(), base.size());
    bool hasGray = false;
    for (int y = 30; y < 50 && !hasGray; ++y) {
        for (int x = 30; x < 90; ++x) {
            const int gray = qGray(rendered.pixel(x, y));
            if (gray > 40 && gray < 215) {
                hasGray = true;
                break;
            }
        }
    }
    QVERIFY(hasGray);
}

QTEST_MAIN(AnnotationDocumentTest)

#include "test_annotation_document.moc"
