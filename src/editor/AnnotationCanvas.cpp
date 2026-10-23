#include "editor/AnnotationCanvas.h"

#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTextEdit>

#include <algorithm>

namespace snapocr::editor {
namespace {

constexpr int kMaxUndoSteps = 100;

bool isDragShape(annotate::AnnotationKind kind)
{
    switch (kind) {
    case annotate::AnnotationKind::Rectangle:
    case annotate::AnnotationKind::Ellipse:
    case annotate::AnnotationKind::Line:
    case annotate::AnnotationKind::Arrow:
    case annotate::AnnotationKind::Highlight:
    case annotate::AnnotationKind::Mosaic:
        return true;
    default:
        return false;
    }
}

}  // namespace

AnnotationCanvas::AnnotationCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setCursor(Qt::CrossCursor);
    m_style.color = QColor(QStringLiteral("#FF3B30"));
    m_style.width = 3;
    m_style.fontSize = 20;
}

void AnnotationCanvas::setDocument(annotate::AnnotationDocument* document)
{
    m_document = document;
    m_undoStack.clear();
    m_redoStack.clear();
    m_lastSnapshot.clear();
    m_selectedIndex = -1;
    if (m_document != nullptr) {
        m_lastSnapshot = m_document->items();
    }
    update();
    emit undoStateChanged();
}

qreal AnnotationCanvas::imageScale() const
{
    if (m_document == nullptr) {
        return 1.0;
    }
    const QSize imageSize = m_document->imageSize();
    if (imageSize.isEmpty() || width() <= 0) {
        return 1.0;
    }
    return static_cast<qreal>(width()) / imageSize.width();
}

QPointF AnnotationCanvas::toImage(const QPointF& widgetPos) const
{
    const qreal scale = imageScale();
    if (scale <= 0.0) {
        return widgetPos;
    }
    QPointF imagePos = widgetPos / scale;
    if (m_document != nullptr) {
        const QSize imageSize = m_document->imageSize();
        imagePos.setX(std::clamp(imagePos.x(), 0.0, static_cast<qreal>(imageSize.width() - 1)));
        imagePos.setY(std::clamp(imagePos.y(), 0.0, static_cast<qreal>(imageSize.height() - 1)));
    }
    return imagePos;
}

void AnnotationCanvas::setTool(annotate::AnnotationKind kind)
{
    m_tool = kind;
    m_selectMode = false;
    setCursor(Qt::CrossCursor);
    if (kind == annotate::AnnotationKind::Text) {
        setCursor(Qt::IBeamCursor);
    }
}

void AnnotationCanvas::setSelectMode(bool select)
{
    m_selectMode = select;
    setCursor(select ? Qt::ArrowCursor : Qt::CrossCursor);
    if (!select) {
        m_selectedIndex = -1;
        update();
    }
}

void AnnotationCanvas::setStyle(const annotate::AnnotationStyle& style)
{
    m_style = style;
}

void AnnotationCanvas::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void AnnotationCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_document == nullptr || m_document->baseImage().isNull()) {
        painter.fillRect(rect(), QColor(0x20, 0x20, 0x20));
        return;
    }

    painter.drawImage(QRectF(rect()), m_document->baseImage());

    const qreal scale = imageScale();
    painter.save();
    painter.scale(scale, scale);
    m_document->paintAll(painter);

    if (m_hasPending) {
        annotate::paintItem(painter, m_pending, m_document->baseImage());
    }

    const QRectF selected = selectedBounds();
    if (!selected.isNull()) {
        QPen pen(QColor(0x1F, 0x8F, 0xEB));
        pen.setStyle(Qt::DashLine);
        pen.setWidthF(1.5 / std::max<qreal>(scale, 0.0001));
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(selected.adjusted(-2, -2, 2, 2));
    }
    painter.restore();
}

QRectF AnnotationCanvas::selectedBounds() const
{
    if (m_document == nullptr || m_selectedIndex < 0
        || m_selectedIndex >= m_document->items().size()) {
        return {};
    }
    return annotate::computeBounds(m_document->items().at(m_selectedIndex));
}

void AnnotationCanvas::mousePressEvent(QMouseEvent* event)
{
    if (m_document == nullptr || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    const QPointF imagePos = toImage(event->position());

    if (m_selectMode) {
        m_selectedIndex = m_document->hitTest(imagePos);
        if (m_selectedIndex >= 0) {
            m_movingSelected = true;
            m_moveStartImage = imagePos;
            m_moveOriginalPoints = m_document->items().at(m_selectedIndex).points;
        }
        update();
        return;
    }

    switch (m_tool) {
    case annotate::AnnotationKind::Text:
        beginTextInput(imagePos);
        return;
    case annotate::AnnotationKind::StepNumber: {
        annotate::AnnotationItem item;
        item.kind = annotate::AnnotationKind::StepNumber;
        item.style = m_style;
        item.stepNumber = m_document->nextStepNumber();
        item.points = {imagePos};
        m_document->addItem(item);
        emit documentChanged();
        commitPending();
        update();
        return;
    }
    case annotate::AnnotationKind::Freehand: {
        m_pending = annotate::AnnotationItem{};
        m_pending.kind = annotate::AnnotationKind::Freehand;
        m_pending.style = m_style;
        m_pending.points = {imagePos};
        m_hasPending = true;
        m_dragging = true;
        return;
    }
    default:
        if (!isDragShape(m_tool)) {
            return;
        }
        m_dragStartImage = imagePos;
        m_pending = annotate::AnnotationItem{};
        m_pending.kind = m_tool;
        m_pending.style = m_style;
        m_pending.points = {imagePos, imagePos};
        m_hasPending = true;
        m_dragging = true;
        return;
    }
}

void AnnotationCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_document == nullptr) {
        return;
    }
    const QPointF imagePos = toImage(event->position());

    if (m_movingSelected && m_selectedIndex >= 0
        && m_selectedIndex < m_document->items().size()) {
        const QPointF delta = imagePos - m_moveStartImage;
        QVector<annotate::AnnotationItem> items = m_document->items();
        annotate::AnnotationItem& item = items[m_selectedIndex];
        item.points = m_moveOriginalPoints;
        for (QPointF& point : item.points) {
            point += delta;
        }
        m_document->setItems(items);
        emit documentChanged();
        update();
        return;
    }

    if (!m_dragging || !m_hasPending) {
        return;
    }

    if (m_pending.kind == annotate::AnnotationKind::Freehand) {
        if (m_pending.points.size() < 5000) {
            m_pending.points.append(imagePos);
        }
    } else if (m_pending.points.size() >= 2) {
        m_pending.points[1] = imagePos;
    }
    update();
}

void AnnotationCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_document == nullptr || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (m_movingSelected) {
        m_movingSelected = false;
        commitPending();
        emit documentChanged();
        return;
    }

    if (!m_dragging || !m_hasPending) {
        return;
    }
    m_dragging = false;

    const QPointF endPos = toImage(event->position());
    if (m_pending.kind == annotate::AnnotationKind::Freehand) {
        if (m_pending.points.size() >= 2) {
            m_document->addItem(m_pending);
            emit documentChanged();
        }
    } else if (m_pending.points.size() >= 2) {
        m_pending.points[1] = endPos;
        const QRectF rect = QRectF(m_pending.points[0], m_pending.points[1]).normalized();
        const bool bigEnough = rect.width() >= 2 || rect.height() >= 2
                               || m_pending.kind == annotate::AnnotationKind::Line
                               || m_pending.kind == annotate::AnnotationKind::Arrow;
        if (bigEnough) {
            m_document->addItem(m_pending);
            emit documentChanged();
        }
    }

    m_hasPending = false;
    m_pending = annotate::AnnotationItem{};
    commitPending();
    update();
}

void AnnotationCanvas::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_textEditor != nullptr) {
            cancelTextInput();
            return;
        }
        emit cancelRequested();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit confirmRequested();
        return;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        deleteSelected();
        return;
    case Qt::Key_S:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            emit saveRequested();
            return;
        }
        break;
    case Qt::Key_C:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            emit confirmRequested();  // 复制并结束（与 Enter 一致）
            return;
        }
        break;
    case Qt::Key_Z:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            undo();
            return;
        }
        break;
    case Qt::Key_Y:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            redo();
            return;
        }
        break;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void AnnotationCanvas::commitPending()
{
    if (m_document == nullptr) {
        return;
    }
    const QVector<annotate::AnnotationItem> current = m_document->items();
    if (current == m_lastSnapshot) {
        return;
    }
    m_undoStack.append(m_lastSnapshot);
    if (m_undoStack.size() > kMaxUndoSteps) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear();
    m_lastSnapshot = current;
    emit undoStateChanged();
}

void AnnotationCanvas::undo()
{
    if (m_document == nullptr || m_undoStack.isEmpty()) {
        return;
    }
    m_redoStack.append(m_lastSnapshot);
    const QVector<annotate::AnnotationItem> previous = m_undoStack.takeLast();
    m_document->setItems(previous);
    m_lastSnapshot = previous;
    m_selectedIndex = -1;
    m_textEditor = nullptr;
    emit documentChanged();
    emit undoStateChanged();
    update();
}

void AnnotationCanvas::redo()
{
    if (m_document == nullptr || m_redoStack.isEmpty()) {
        return;
    }
    m_undoStack.append(m_lastSnapshot);
    const QVector<annotate::AnnotationItem> next = m_redoStack.takeLast();
    m_document->setItems(next);
    m_lastSnapshot = next;
    m_selectedIndex = -1;
    emit documentChanged();
    emit undoStateChanged();
    update();
}

bool AnnotationCanvas::eventFilter(QObject* watched, QEvent* event)
{
    if (m_textEditor != nullptr && watched == m_textEditor) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
                    commitTextInput();
                    return true;
                }
                return false;  // 普通回车换行
            }
            if (keyEvent->key() == Qt::Key_Escape) {
                cancelTextInput();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            // 点击画布其他位置时提交文本。
            commitTextInput();
            return false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool AnnotationCanvas::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool AnnotationCanvas::canRedo() const
{
    return !m_redoStack.isEmpty();
}

void AnnotationCanvas::deleteSelected()
{
    if (m_document == nullptr || m_selectedIndex < 0) {
        return;
    }
    m_document->removeItem(m_selectedIndex);
    m_selectedIndex = -1;
    emit documentChanged();
    commitPending();
    update();
}

void AnnotationCanvas::beginTextInput(const QPointF& imagePos)
{
    if (m_document == nullptr) {
        return;
    }
    cancelTextInput();

    m_textImagePos = imagePos;
    auto* editor = new QTextEdit(this);
    m_textEditor = editor;
    editor->setObjectName(QStringLiteral("snapocrTextEditor"));
    editor->setAcceptRichText(false);
    editor->setFrameShape(QFrame::Box);

    const qreal scale = imageScale();
    const int logicalWidth = std::clamp(static_cast<int>(220 * scale), 80, width());
    const int logicalHeight = std::clamp(static_cast<int>(80 * scale), 40, height());
    const QPoint pos(static_cast<int>(imagePos.x() * scale), static_cast<int>(imagePos.y() * scale));
    editor->setGeometry(pos.x(), pos.y(), logicalWidth, logicalHeight);
    editor->setStyleSheet(QStringLiteral(
        "QTextEdit { background-color: rgba(0, 0, 0, 190); color: white;"
        " border: 1px solid #1F8FEB; font-size: 14px; }"));
    editor->show();
    editor->setFocus();

    editor->installEventFilter(this);
    // 必须校验身份：旧输入框是 deleteLater 销毁的，它的 destroyed 会在新输入框
    // 创建之后才触发；直接无条件置空会把新输入框变成“野控件”（还显示着但已不受管理），
    // 表现为画布上残留一个空文本框。
    connect(editor, &QTextEdit::destroyed, this, [this, editor]() {
        if (m_textEditor == editor) {
            m_textEditor = nullptr;
        }
    });
}

void AnnotationCanvas::commitOrDiscardTextInput()
{
    if (m_textEditor == nullptr) {
        return;
    }
    if (m_textEditor->toPlainText().trimmed().isEmpty()) {
        cancelTextInput();  // 空输入：直接丢弃，画布上不留任何痕迹
    } else {
        commitTextInput();
    }
}

void AnnotationCanvas::commitTextInput()
{
    if (m_textEditor == nullptr || m_document == nullptr) {
        return;
    }
    const QString text = m_textEditor->toPlainText();
    QTextEdit* editor = m_textEditor;
    m_textEditor = nullptr;
    // deleteLater 之前先隐藏：否则在事件循环处理前，这个子控件仍会挡住画布点击。
    editor->hide();
    editor->setEnabled(false);
    editor->deleteLater();

    if (text.trimmed().isEmpty()) {
        update();
        return;
    }

    annotate::AnnotationItem item;
    item.kind = annotate::AnnotationKind::Text;
    item.style = m_style;
    item.text = text;
    item.points = {m_textImagePos};
    m_document->addItem(item);
    emit documentChanged();
    commitPending();
    update();
}

void AnnotationCanvas::cancelTextInput()
{
    if (m_textEditor == nullptr) {
        return;
    }
    QTextEdit* editor = m_textEditor;
    m_textEditor = nullptr;
    // deleteLater 之前先隐藏：否则在事件循环处理前，这个子控件仍会挡住画布点击。
    editor->hide();
    editor->setEnabled(false);
    editor->deleteLater();
    update();
}

}  // namespace snapocr::editor
