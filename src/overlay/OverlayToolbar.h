#pragma once

#include <QColor>
#include <QWidget>

#include "annotate/AnnotationTypes.h"

class QButtonGroup;
class QToolButton;

namespace snapocr::overlay {

// 截图工具条：选区确认后浮在选区旁。
// 选择模式：取字 / 保存 / 复制 / 取消；点击任一标注工具进入编辑模式。
class OverlayToolbar final : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayToolbar(QWidget* parent = nullptr);

    void setEditingMode(bool editing);
    void setSelectMode(bool select);
    void clearToolSelection();
    void setUndoRedoEnabled(bool canUndo, bool canRedo);
    void setCurrentTool(annotate::AnnotationKind kind);
    void setCurrentColor(const QColor& color);
    void setCurrentWidth(int width);
    bool isEditingMode() const { return m_editing; }

    // 定位到选区附近（物理像素矩形 + 全局逻辑矩形）。
    void positionNear(const QRect& physicalSelection, const QRect& globalLogicalSelection,
                      const QRect& virtualLogicalDesktop);

signals:
    void toolSelected(snapocr::annotate::AnnotationKind kind);
    void selectToolSelected();
    void copyRequested();
    void pinRequested();
    void saveRequested();
    void ocrRequested();
    void cancelRequested();
    void undoRequested();
    void redoRequested();
    void colorChangeRequested(const QColor& color);
    void widthChangeRequested(int width);
    void fillToggled(bool filled);
    void dashToggled(bool dashed);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyStyleSheet();
    void hideTooltipNow();
    void scheduleTooltip(QWidget* button);
    void cancelTooltip();

    class QTimer* m_hoverTimer = nullptr;
    QWidget* m_hoverButton = nullptr;

    QWidget* m_toolGroup = nullptr;
    QWidget* m_styleGroup = nullptr;
    QWidget* m_undoGroup = nullptr;
    QToolButton* m_selectButton = nullptr;
    QToolButton* m_undoButton = nullptr;
    QToolButton* m_redoButton = nullptr;
    QButtonGroup* m_toolGroupBox = nullptr;
    QToolButton* m_colorButton = nullptr;
    QToolButton* m_widthButton = nullptr;
    QToolButton* m_fillButton = nullptr;
    QToolButton* m_dashButton = nullptr;
    QVector<QToolButton*> m_toolButtons;
    bool m_editing = false;
    bool m_selectDrawn = false;
    int m_toolbarWidth = 0;
};

}  // namespace snapocr::overlay
