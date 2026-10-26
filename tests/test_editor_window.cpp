#include <QtTest>

#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QSignalSpy>

#include "annotate/AnnotationDocument.h"
#include "editor/AnnotationCanvas.h"
#include "editor/EditorWindow.h"

using namespace snapocr;

// 编辑窗集成测试：验证贴图/截图进入编辑器后的窗口级交互
// （Enter 复制、Esc 取消、Ctrl+S 保存、渲染尺寸与定位约束）。
class EditorWindowTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void renderReturnsBaseImage();
    void enterRequestsConfirm();
    void escapeRequestsCancel();
    void ctrlSRequestsSave();
    void placeAtEnforcesMinimumSize();

private:
    QImage makeBase() const;

    annotate::AnnotationDocument* m_document = nullptr;
    editor::EditorWindow* m_window = nullptr;
};

QImage EditorWindowTest::makeBase() const
{
    QImage image(320, 200, QImage::Format_ARGB32);
    image.fill(QColor(0x20, 0x80, 0xC0));
    return image;
}

void EditorWindowTest::init()
{
    m_document = new annotate::AnnotationDocument;
    m_document->setBaseImage(makeBase());
    m_window = new editor::EditorWindow;
    m_window->setDocument(m_document);
    m_window->resize(320, 200);
    m_window->show();
}

void EditorWindowTest::cleanup()
{
    delete m_window;
    m_window = nullptr;
    delete m_document;
    m_document = nullptr;
}

void EditorWindowTest::renderReturnsBaseImage()
{
    const QImage rendered = m_window->render();
    QCOMPARE(rendered.size(), QSize(320, 200));
    QCOMPARE(rendered.pixelColor(10, 10), QColor(0x20, 0x80, 0xC0));
}

void EditorWindowTest::enterRequestsConfirm()
{
    QSignalSpy spy(m_window, &editor::EditorWindow::confirmRequested);
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(m_window, &event);
    QCOMPARE(spy.count(), 1);
}

void EditorWindowTest::escapeRequestsCancel()
{
    QSignalSpy spy(m_window, &editor::EditorWindow::cancelRequested);
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(m_window, &event);
    QCOMPARE(spy.count(), 1);
}

void EditorWindowTest::ctrlSRequestsSave()
{
    QSignalSpy spy(m_window, &editor::EditorWindow::saveRequested);
    QKeyEvent event(QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier);
    QApplication::sendEvent(m_window, &event);
    QCOMPARE(spy.count(), 1);
}

void EditorWindowTest::placeAtEnforcesMinimumSize()
{
    m_window->placeAt(QRect(100, 100, 4, 4));  // 小于最小尺寸
    QVERIFY(m_window->width() >= 24);
    QVERIFY(m_window->height() >= 24);

    m_window->placeAt(QRect(50, 60, 320, 200));
    QCOMPARE(m_window->geometry(), QRect(50, 60, 320, 200));
}

QTEST_MAIN(EditorWindowTest)

#include "test_editor_window.moc"
