#include "editor/EditorWindow.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>

#include "editor/AnnotationCanvas.h"

namespace snapocr::editor {

EditorWindow::EditorWindow(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)

{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(QStringLiteral("SnapOCR 编辑"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    m_canvas = new AnnotationCanvas(this);
    layout->addWidget(m_canvas);

    connect(m_canvas, &AnnotationCanvas::confirmRequested, this, &EditorWindow::confirmRequested);
    connect(m_canvas, &AnnotationCanvas::cancelRequested, this, &EditorWindow::cancelRequested);
    connect(m_canvas, &AnnotationCanvas::saveRequested, this, &EditorWindow::saveRequested);
}

void EditorWindow::setDocument(annotate::AnnotationDocument* document)
{
    m_canvas->setDocument(document);
}

void EditorWindow::setTool(annotate::AnnotationKind kind)
{
    m_canvas->setTool(kind);
    m_canvas->setFocus();
}

void EditorWindow::setSelectMode(bool select)
{
    m_canvas->setSelectMode(select);
    m_canvas->setFocus();
}

void EditorWindow::setStyle(const annotate::AnnotationStyle& style)
{
    m_canvas->setStyle(style);
}

void EditorWindow::undo()
{
    m_canvas->undo();
}

void EditorWindow::redo()
{
    m_canvas->redo();
}

QImage EditorWindow::render() const
{
    if (m_canvas == nullptr || m_canvas->document() == nullptr) {
        return {};
    }
    return m_canvas->document()->render();
}

void EditorWindow::placeAt(const QRect& globalLogicalSelection)
{
    QRect geometry = globalLogicalSelection;
    if (geometry.width() < 24) {
        geometry.setWidth(24);
    }
    if (geometry.height() < 24) {
        geometry.setHeight(24);
    }
    setGeometry(geometry);
}

void EditorWindow::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
}

void EditorWindow::keyPressEvent(QKeyEvent* event)
{
    // 兜底：若焦点落在窗口而不是画布上，仍然响应确认/取消。
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit confirmRequested();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        emit cancelRequested();
        return;
    }
    if (event->key() == Qt::Key_S && event->modifiers().testFlag(Qt::ControlModifier)) {
        emit saveRequested();
        return;
    }
    QWidget::keyPressEvent(event);
}

void EditorWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setPen(QPen(QColor(0x1F, 0x8F, 0xEB), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

}  // namespace snapocr::editor
