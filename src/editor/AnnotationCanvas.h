#pragma once

#include <QPointer>
#include <QWidget>

#include <QTextEdit>

#include "annotate/AnnotationDocument.h"
#include "annotate/AnnotationTypes.h"

namespace snapocr::editor {

// 标注画布：按图像物理像素与控件逻辑像素换算绘制，图元坐标存图像像素。
class AnnotationCanvas final : public QWidget
{
    Q_OBJECT

public:
    explicit AnnotationCanvas(QWidget* parent = nullptr);

    void setDocument(annotate::AnnotationDocument* document);
    annotate::AnnotationDocument* document() const { return m_document; }

    void setTool(annotate::AnnotationKind kind);
    void setSelectMode(bool select);
    annotate::AnnotationKind tool() const { return m_tool; }
    bool isSelectMode() const { return m_selectMode; }

    void setStyle(const annotate::AnnotationStyle& style);
    annotate::AnnotationStyle style() const { return m_style; }

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    void deleteSelected();

signals:
    void documentChanged();
    void undoStateChanged();
    void confirmRequested();
    void cancelRequested();
    void saveRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    qreal imageScale() const;
    QPointF toImage(const QPointF& widgetPos) const;
    void commitPending();
    void beginTextInput(const QPointF& imagePos);
    void commitTextInput();
    void cancelTextInput();
    QRectF selectedBounds() const;

    annotate::AnnotationDocument* m_document = nullptr;
    annotate::AnnotationKind m_tool = annotate::AnnotationKind::Rectangle;
    annotate::AnnotationStyle m_style;
    bool m_selectMode = false;

    bool m_dragging = false;
    QPointF m_dragStartImage;
    annotate::AnnotationItem m_pending;
    bool m_hasPending = false;

    int m_selectedIndex = -1;
    bool m_movingSelected = false;
    QPointF m_moveStartImage;
    QVector<QPointF> m_moveOriginalPoints;

    QPointer<QTextEdit> m_textEditor;
    QPointF m_textImagePos;

    QVector<QVector<annotate::AnnotationItem>> m_undoStack;
    QVector<QVector<annotate::AnnotationItem>> m_redoStack;
    QVector<annotate::AnnotationItem> m_lastSnapshot;
};

}  // namespace snapocr::editor
