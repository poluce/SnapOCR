#pragma once

#include <QImage>
#include <QRect>
#include <QVector>
#include <QWidget>

#include "annotate/AnnotationDocument.h"
#include "annotate/AnnotationTypes.h"
#include "capture/CaptureService.h"
#include "capture/CaptureTypes.h"
#include "overlay/SelectionModel.h"

namespace snapocr::editor {
class EditorWindow;
}

namespace snapocr::overlay {

class OverlayToolbar;
class OverlayWindow;

// 截图主流程编排：捕获 → 逐屏覆盖层 → 选区 → 工具条 → 标注编辑 → 输出。
// 输出动作（复制/保存/OCR）通过信号上抛给 Application，本类不直接落盘或识别。
class OverlayController : public QObject
{
    Q_OBJECT

public:
    explicit OverlayController(QObject* parent = nullptr);
    ~OverlayController() override;

    void beginCapture();
    void beginOcrCapture();
    void cancel();

    // 覆盖层激活期间由全局热键（Esc/Enter）调用，规避窗口无焦点时收不到按键的问题。
    void requestConfirm();
    void requestCancel();

    bool isActive() const { return m_active; }
    bool isEditing() const { return m_editor != nullptr; }

    void setAnnotationStyle(const annotate::AnnotationStyle& style);
    annotate::AnnotationStyle annotationStyle() const { return m_style; }

signals:
    void started();
    void copyRequested(const QImage& image);
    void saveRequested(const QImage& image);
    void ocrRequested(const QImage& image);
    void finished();
    void errorOccurred(const QString& message);

private:
    void onCaptured(const snapocr::capture::CaptureResult& result);
    void onCaptureFailed(const QString& message);
    void buildOverlays();
    void destroyOverlays();
    void showToolbar();
    void raiseToolbar();
    void hideToolbar();
    void placeToolbar();

    void onSelectionChanged();
    void onSelectionReleased();
    void enterEditingMode(annotate::AnnotationKind kind);
    void exitEditingMode();

    QRect selectionPhysical() const;
    QRect selectionGlobalLogical() const;
    QImage currentImage() const;
    QImage croppedSelection() const;

    void requestCopy();
    void finishPendingTextInput();
    void requestSave();
    void requestOcr();
    void finish();
    void focusOverlay();

    capture::CaptureService* m_captureService = nullptr;
    capture::CaptureResult m_result;
    QVector<capture::MonitorInfo> m_monitors;
    SelectionModel m_selection;

    QVector<OverlayWindow*> m_overlays;
    OverlayToolbar* m_toolbar = nullptr;
    editor::EditorWindow* m_editor = nullptr;

    annotate::AnnotationDocument m_document;
    annotate::AnnotationStyle m_style;

    QRect m_virtualLogicalDesktop;
    bool m_active = false;
    bool m_ocrMode = false;
};

}  // namespace snapocr::overlay
