#include <QtTest>

#include <QApplication>
#include <QImage>

#include "annotate/AnnotationDocument.h"
#include "editor/AnnotationCanvas.h"

using namespace snapocr;
using namespace snapocr::editor;

namespace {

QImage makeBaseImage(int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::white);
    return image;
}

}  // namespace

class AnnotationCanvasTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void drawsRectangleByDrag();
    void drawsLineByDrag();
    void drawsFreehandPath();
    void undoRemovesLastItem();
    void redoRestoresItem();
    void deleteRemovesSelectedItem();
    void stepNumberIncrements();
    void switchingToolKeepsExistingItems();
    void resettingBaseImageClearsItems();

private:
    void dragFrom(const QPoint& from, const QPoint& to);

    annotate::AnnotationDocument m_document;
    AnnotationCanvas* m_canvas = nullptr;
};

void AnnotationCanvasTest::init()
{
    m_document.setBaseImage(makeBaseImage(800, 600));
    m_canvas = new AnnotationCanvas;
    m_canvas->resize(400, 300);  // 逻辑尺寸 = 图像的一半，验证坐标换算
    m_canvas->setDocument(&m_document);
    m_canvas->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_canvas));
}

void AnnotationCanvasTest::cleanup()
{
    delete m_canvas;
    m_canvas = nullptr;
}

void AnnotationCanvasTest::dragFrom(const QPoint& from, const QPoint& to)
{
    QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier, from);
    // 分几步移动，模拟真实拖拽
    for (int i = 1; i <= 4; ++i) {
        const QPoint step(from.x() + (to.x() - from.x()) * i / 4,
                          from.y() + (to.y() - from.y()) * i / 4);
        QTest::mouseMove(m_canvas, step);
    }
    QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier, to);
    QTest::qWait(10);
}

void AnnotationCanvasTest::drawsRectangleByDrag()
{
    m_canvas->setTool(annotate::AnnotationKind::Rectangle);
    dragFrom(QPoint(50, 50), QPoint(200, 150));

    QCOMPARE(m_document.items().size(), 1);
    const annotate::AnnotationItem& item = m_document.items().first();
    QCOMPARE(item.kind, annotate::AnnotationKind::Rectangle);
    QCOMPARE(item.points.size(), 2);
    // 控件坐标 50..200 → 图像坐标 100..400（比例 0.5）
    QCOMPARE(item.points.first().toPoint(), QPoint(100, 100));
    QCOMPARE(item.points.last().toPoint(), QPoint(400, 300));
}

void AnnotationCanvasTest::drawsLineByDrag()
{
    m_canvas->setTool(annotate::AnnotationKind::Line);
    dragFrom(QPoint(20, 20), QPoint(180, 120));
    QCOMPARE(m_document.items().size(), 1);
    QCOMPARE(m_document.items().first().kind, annotate::AnnotationKind::Line);
}

void AnnotationCanvasTest::drawsFreehandPath()
{
    m_canvas->setTool(annotate::AnnotationKind::Freehand);
    QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 30));
    for (int i = 0; i < 6; ++i) {
        QTest::mouseMove(m_canvas, QPoint(30 + i * 20, 30 + i * 10));
    }
    QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 90));
    QTest::qWait(10);

    QCOMPARE(m_document.items().size(), 1);
    QCOMPARE(m_document.items().first().kind, annotate::AnnotationKind::Freehand);
    QVERIFY(m_document.items().first().points.size() >= 3);
}

void AnnotationCanvasTest::undoRemovesLastItem()
{
    m_canvas->setTool(annotate::AnnotationKind::Rectangle);
    dragFrom(QPoint(20, 20), QPoint(80, 80));
    dragFrom(QPoint(100, 100), QPoint(160, 160));
    QCOMPARE(m_document.items().size(), 2);

    m_canvas->undo();
    QCOMPARE(m_document.items().size(), 1);
    m_canvas->undo();
    QCOMPARE(m_document.items().size(), 0);
    QVERIFY(!m_canvas->canUndo());
}

void AnnotationCanvasTest::redoRestoresItem()
{
    m_canvas->setTool(annotate::AnnotationKind::Rectangle);
    dragFrom(QPoint(20, 20), QPoint(80, 80));
    m_canvas->undo();
    QCOMPARE(m_document.items().size(), 0);
    QVERIFY(m_canvas->canRedo());

    m_canvas->redo();
    QCOMPARE(m_document.items().size(), 1);
    QVERIFY(!m_canvas->canRedo());
}

void AnnotationCanvasTest::deleteRemovesSelectedItem()
{
    m_canvas->setTool(annotate::AnnotationKind::Rectangle);
    dragFrom(QPoint(20, 20), QPoint(80, 80));
    QCOMPARE(m_document.items().size(), 1);

    m_canvas->setSelectMode(true);
    // 点击矩形边框（图像坐标 (40,40) → 控件 (20,20)）
    QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    m_canvas->deleteSelected();
    QCOMPARE(m_document.items().size(), 0);
}

void AnnotationCanvasTest::stepNumberIncrements()
{
    m_canvas->setTool(annotate::AnnotationKind::StepNumber);
    QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 40));
    QTest::qWait(10);
    QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(80, 80));
    QTest::qWait(10);

    QCOMPARE(m_document.items().size(), 2);
    QCOMPARE(m_document.items().at(0).stepNumber, 1);
    QCOMPARE(m_document.items().at(1).stepNumber, 2);
}

// 回归：画完矩形后切换椭圆工具，已画的矩形必须保留。
// （曾因 OverlayController 每次点工具都重新 setBaseImage 导致图元被清空）
void AnnotationCanvasTest::switchingToolKeepsExistingItems()
{
    m_canvas->setTool(annotate::AnnotationKind::Rectangle);
    dragFrom(QPoint(30, 30), QPoint(120, 90));
    QCOMPARE(m_document.items().size(), 1);

    m_canvas->setTool(annotate::AnnotationKind::Ellipse);
    dragFrom(QPoint(140, 120), QPoint(220, 180));

    QCOMPARE(m_document.items().size(), 2);
    QCOMPARE(m_document.items().at(0).kind, annotate::AnnotationKind::Rectangle);
    QCOMPARE(m_document.items().at(1).kind, annotate::AnnotationKind::Ellipse);

    // 再切第三次工具，仍然全部保留
    m_canvas->setTool(annotate::AnnotationKind::Arrow);
    QCOMPARE(m_document.items().size(), 2);
}

// 文档语义：setBaseImage() 表示“载入新的底图”，会清空图元。
// 因此编辑器只在首次进入标注时调用一次，切换工具时禁止再调用。
void AnnotationCanvasTest::resettingBaseImageClearsItems()
{
    m_canvas->setTool(annotate::AnnotationKind::Rectangle);
    dragFrom(QPoint(30, 30), QPoint(120, 90));
    QCOMPARE(m_document.items().size(), 1);

    m_document.setBaseImage(makeBaseImage(800, 600));
    QCOMPARE(m_document.items().size(), 0);
}

QTEST_MAIN(AnnotationCanvasTest)

#include "test_annotation_canvas.moc"
