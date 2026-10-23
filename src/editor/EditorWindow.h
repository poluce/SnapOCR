#pragma once

#include <QWidget>

#include "annotate/AnnotationDocument.h"
#include "annotate/AnnotationTypes.h"

namespace snapocr::editor {

class AnnotationCanvas;

// 标注编辑窗：无边框置顶，定位在选区之上，承载标注画布。
class EditorWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit EditorWindow(QWidget* parent = nullptr);

    void setDocument(annotate::AnnotationDocument* document);
    AnnotationCanvas* canvas() const { return m_canvas; }

    QImage render() const;

    void setTool(annotate::AnnotationKind kind);
    void setSelectMode(bool select);
    void setStyle(const annotate::AnnotationStyle& style);
    void undo();
    void redo();

    // 选区（全局逻辑坐标）→ 窗口几何。
    void placeAt(const QRect& globalLogicalSelection);

signals:
    void confirmRequested();
    void cancelRequested();
    void saveRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    AnnotationCanvas* m_canvas = nullptr;
};

}  // namespace snapocr::editor
