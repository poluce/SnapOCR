#include <QtTest>

#include <QApplication>
#include <QImage>
#include <QTextEdit>

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
    void emptyTextInputLeavesNoItem();
    void typedTextInputCommitsOneItem();
    void clickingNewPositionDiscardsEmptyTextInput();
    void resettingBaseImageClearsItems();
    void resizeHandleDragScalesItem();
    void ctrlBracketChangesLayerOrder();
    void drawsBlurStroke();

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

// 文字工具：点了位置但没输入文字，不应产生任何图元。
void AnnotationCanvasTest::emptyTextInputLeavesNoItem()
{
    m_canvas->setTool(annotate::AnnotationKind::Text);
    QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 60));
    QTest::qWait(30);
    QVERIFY(m_canvas->isTextInputActive());

    m_canvas->commitOrDiscardTextInput();
    QVERIFY(!m_canvas->isTextInputActive());
    QCOMPARE(m_document.items().size(), 0);
}

// 文字工具：输入内容后提交，得到一个 Text 图元。
void AnnotationCanvasTest::typedTextInputCommitsOneItem()
{
    m_canvas->setTool(annotate::AnnotationKind::Text);
    QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(70, 70));
    QTest::qWait(30);
    QVERIFY(m_canvas->isTextInputActive());

    auto* editor = m_canvas->findChild<QTextEdit*>(QStringLiteral("snapocrTextEditor"));
    QVERIFY(editor != nullptr);
    editor->setPlainText(QStringLiteral("测试文字"));

    m_canvas->commitOrDiscardTextInput();
    QCOMPARE(m_document.items().size(), 1);
    QCOMPARE(m_document.items().first().kind, annotate::AnnotationKind::Text);
    QCOMPARE(m_document.items().first().text, QStringLiteral("测试文字"));
}

// 空输入状态下点击画布另一个位置：上一个空文本框必须被丢弃，不残留。
void AnnotationCanvasTest::clickingNewPositionDiscardsEmptyTextInput()
{
    m_canvas->setTool(annotate::AnnotationKind::Text);
    QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));
    QTest::qWait(30);
    QVERIFY(m_canvas->isTextInputActive());

    // 在画布别处再点一下（模拟“没输入文字就换位置”）
    m_canvas->commitOrDiscardTextInput();
    QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(350, 270));
    QTest::qWait(30);

    QCOMPARE(m_document.items().size(), 0);
    QVERIFY(m_canvas->isTextInputActive());  // 新位置打开了一个新的输入框
    m_canvas->commitOrDiscardTextInput();
    QCOMPARE(m_document.items().size(), 0);
}

void AnnotationCanvasTest::resizeHandleDragScalesItem()
{
    // 画一个矩形（控件 50,50 → 200,150；图像 100,100 → 400,300）
    m_canvas->setTool(annotate::AnnotationKind::Rectangle);
    dragFrom(QPoint(50, 50), QPoint(200, 150));
    QCOMPARE(m_document.items().size(), 1);

    // 切到选择模式并点中图元
    m_canvas->setSelectMode(true);
    QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(120, 100));
    QTest::qWait(10);

    const QRectF before = annotate::computeBounds(m_document.items().at(0));

    // 从右下控制点拖到 (250,200)：图像坐标 (500,400)
    QTest::mousePress(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(201, 151));
    for (int i = 1; i <= 4; ++i) {
        QTest::mouseMove(m_canvas, QPoint(201 + (250 - 201) * i / 4, 151 + (200 - 151) * i / 4));
    }
    QTest::mouseRelease(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(250, 200));
    QTest::qWait(10);

    const QRectF after = annotate::computeBounds(m_document.items().at(0));
    QVERIFY(after.width() > before.width());
    QVERIFY(after.height() > before.height());
    // 右下角应接近图像坐标 500,400（允许几个像素的控制点误差）
    QVERIFY(qAbs(after.right() - 500.0) < 12.0);
    QVERIFY(qAbs(after.bottom() - 400.0) < 12.0);
}

void AnnotationCanvasTest::ctrlBracketChangesLayerOrder()
{
    annotate::AnnotationItem first;
    first.kind = annotate::AnnotationKind::Rectangle;
    first.points = {QPointF(10, 10), QPointF(60, 60)};
    m_document.addItem(first);

    annotate::AnnotationItem second;
    second.kind = annotate::AnnotationKind::Ellipse;
    second.points = {QPointF(10, 10), QPointF(60, 60)};
    m_document.addItem(second);

    m_canvas->setSelectMode(true);
    // 点中区域：命中测试返回最上层（椭圆），再 Ctrl+] 也无法再上移；
    // 因此先点两次确保选中后，用 Ctrl+Shift+] 置顶 + Ctrl+Shift+[ 置底验证。
    QTest::mouseClick(m_canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    QTest::qWait(10);

    QKeyEvent topEvent(QEvent::KeyPress, Qt::Key_BracketRight,
                       Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(m_canvas, &topEvent);
    QTest::qWait(10);
    QCOMPARE(m_document.items().size(), 2);

    // 选中顶层（椭圆），Ctrl+Shift+[ 应把它置底
    QKeyEvent bottomEvent(QEvent::KeyPress, Qt::Key_BracketLeft,
                          Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(m_canvas, &bottomEvent);
    QTest::qWait(10);
    QCOMPARE(m_document.items().size(), 2);
    QCOMPARE(m_document.items().first().kind, annotate::AnnotationKind::Ellipse);
    QCOMPARE(m_document.items().last().kind, annotate::AnnotationKind::Rectangle);

    // 再 Ctrl+Shift+] 置顶回最上层
    QKeyEvent topAgain(QEvent::KeyPress, Qt::Key_BracketRight,
                       Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(m_canvas, &topAgain);
    QTest::qWait(10);
    QCOMPARE(m_document.items().last().kind, annotate::AnnotationKind::Ellipse);
}

void AnnotationCanvasTest::drawsBlurStroke()
{
    m_canvas->setTool(annotate::AnnotationKind::Blur);
    dragFrom(QPoint(30, 30), QPoint(150, 90));
    QCOMPARE(m_document.items().size(), 1);
    const annotate::AnnotationItem& item = m_document.items().first();
    QCOMPARE(item.kind, annotate::AnnotationKind::Blur);
    QVERIFY(item.points.size() >= 2);
    QVERIFY(item.style.width >= 3);  // 笔刷宽度按线宽放大
}

QTEST_MAIN(AnnotationCanvasTest)

#include "test_annotation_canvas.moc"
