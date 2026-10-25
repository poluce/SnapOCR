#include "overlay/OverlayController.h"

#include "ui/ImageDrag.h"

#include "ocr/OcrHints.h"

#include <QApplication>
#include <QCursor>
#include <QTimer>

#include <QGuiApplication>
#include <QScreen>

#include "capture/CaptureCrop.h"
#include "capture/CoordinateMap.h"
#include "editor/AnnotationCanvas.h"
#include "editor/EditorWindow.h"
#include "platform/win32/WindowActivation.h"
#include "overlay/OverlayToolbar.h"
#include "overlay/OverlayWindow.h"

namespace snapocr::overlay {

OverlayController::OverlayController(QObject* parent)
    : QObject(parent)
{
    m_captureService = new capture::CaptureService(this);
    connect(m_captureService, &capture::CaptureService::captured, this,
            &OverlayController::onCaptured);
    connect(m_captureService, &capture::CaptureService::failed, this,
            &OverlayController::onCaptureFailed);
}

OverlayController::~OverlayController()
{
    destroyOverlays();
}

void OverlayController::setAnnotationStyle(const annotate::AnnotationStyle& style)
{
    m_style = style;
}

// 放大镜：每 50ms 查询一次光标位置。覆盖层只有在拖拽（有鼠标捕获）时才会收到
// 鼠标移动事件，靠事件驱动会导致“不拖拽时就看不到放大镜”。
void OverlayController::startMagnifierPolling()
{
    if (m_magnifierTimer == nullptr) {
        m_magnifierTimer = new QTimer(this);
        m_magnifierTimer->setInterval(50);
        connect(m_magnifierTimer, &QTimer::timeout, this,
                &OverlayController::updateMagnifierFromCursor);
    }
    m_magnifierTimer->start();
}

void OverlayController::stopMagnifierPolling()
{
    if (m_magnifierTimer != nullptr) {
        m_magnifierTimer->stop();
    }
    for (OverlayWindow* window : m_overlays) {
        if (window != nullptr) {
            window->hideMagnifier();
            window->clearWindowHover();
        }
    }
}

void OverlayController::updateMagnifierFromCursor()
{
    if (!m_active || m_ocrMode || m_overlays.isEmpty()) {
        return;
    }
    const QPoint globalLogical = QCursor::pos();
    for (OverlayWindow* window : m_overlays) {
        if (window == nullptr || !window->isSelectionInteractive()) {
            continue;
        }
        if (window->monitor().logicalGeometry.contains(globalLogical)) {
            window->updateWindowHover(globalLogical);
            window->showMagnifierAt(globalLogical);
            return;
        }
    }
    for (OverlayWindow* window : m_overlays) {
        if (window != nullptr) {
            window->hideMagnifier();
            window->clearWindowHover();
        }
    }
}

void OverlayController::setIncludeCursor(bool include)
{
    m_includeCursor = include;
}

void OverlayController::setDetectWindows(bool enabled)
{
    m_detectWindows = enabled;
    for (OverlayWindow* window : m_overlays) {
        if (window != nullptr) {
            window->setDetectWindows(enabled);
            window->clearWindowHover();
        }
    }
}

void OverlayController::beginCapture()
{
    if (m_active) {
        return;
    }
    m_ocrMode = false;
    m_document.clearItems();
    m_captureService->captureAllScreens(m_includeCursor);
}

void OverlayController::beginOcrCapture()
{
    if (m_active) {
        return;
    }
    m_ocrMode = true;
    m_document.clearItems();
    m_captureService->captureAllScreens(m_includeCursor);
}

void OverlayController::requestConfirm()
{
    if (!m_active) {
        return;
    }
    requestCopy();
}

void OverlayController::requestCancel()
{
    cancel();
}

void OverlayController::cancel()
{
    if (!m_active) {
        return;
    }
    finish();
}

void OverlayController::onCaptureFailed(const QString& message)
{
    m_active = false;
    m_ocrMode = false;
    emit errorOccurred(message);
    emit finished();
}

void OverlayController::onCaptured(const capture::CaptureResult& result)
{
    m_result = result;
    m_monitors.clear();
    for (const capture::ScreenFrame& frame : m_result.screens) {
        m_monitors.append(frame.monitor);
    }

    m_selection.reset();
    m_selection.setBounds(m_result.virtualGeometry);

    // 全局逻辑桌面范围（用于工具条/编辑器定位）。
    bool first = true;
    for (const capture::MonitorInfo& monitor : m_monitors) {
        m_virtualLogicalDesktop = first ? monitor.logicalGeometry
                                        : m_virtualLogicalDesktop.united(monitor.logicalGeometry);
        first = false;
    }

    m_active = true;
    buildOverlays();
    startMagnifierPolling();
    emit started();
    showToolbar();
    hideToolbar();  // 先隐藏，等选区出现再显示
    focusOverlay();
}

void OverlayController::buildOverlays()
{
    destroyOverlays();
    for (const capture::ScreenFrame& frame : m_result.screens) {
        auto* window = new OverlayWindow(frame, &m_monitors, &m_selection);
        window->setDetectWindows(m_detectWindows);
        connect(window, &OverlayWindow::selectionChanged, this,
                &OverlayController::onSelectionChanged);
        connect(window, &OverlayWindow::selectionReleased, this,
                &OverlayController::onSelectionReleased);
        connect(window, &OverlayWindow::selectionConfirmed, this,
                &OverlayController::requestCopy);
        connect(window, &OverlayWindow::saveRequested, this, &OverlayController::requestSave);
        connect(window, &OverlayWindow::dragOutRequested, this,
                &OverlayController::handleDragOut);
        connect(window, &OverlayWindow::colorCopyRequested, this,
                [this](const QColor& color) { emit colorPicked(color.name(QColor::HexRgb).toUpper()); });
        connect(window, &OverlayWindow::copyRequested, this, &OverlayController::requestCopy);
        connect(window, &OverlayWindow::cancelled, this, &OverlayController::cancel);
        window->show();
        window->raise();
        m_overlays.append(window);
    }
}

void OverlayController::destroyOverlays()
{
    for (OverlayWindow* window : m_overlays) {
        window->hide();
        window->deleteLater();
    }
    m_overlays.clear();
}

void OverlayController::showToolbar()
{
    if (m_toolbar == nullptr) {
        m_toolbar = new OverlayToolbar;
        // 工具条不抢键盘焦点：Enter/Esc 始终由覆盖层或编辑器处理。
        m_toolbar->setAttribute(Qt::WA_ShowWithoutActivating, true);

        connect(m_toolbar, &OverlayToolbar::toolSelected, this,
                &OverlayController::enterEditingMode);
        connect(m_toolbar, &OverlayToolbar::selectToolSelected, this, [this]() {
            finishPendingTextInput();
            m_toolbar->setSelectMode(true);
            if (m_editor != nullptr) {
                m_editor->setSelectMode(true);
            }
        });
        connect(m_toolbar, &OverlayToolbar::copyRequested, this, &OverlayController::requestCopy);
        connect(m_toolbar, &OverlayToolbar::pinRequested, this, &OverlayController::requestPin);
        connect(m_toolbar, &OverlayToolbar::scrollingRequested, this,
                &OverlayController::requestScrolling);
        connect(m_toolbar, &OverlayToolbar::saveRequested, this, &OverlayController::requestSave);
        connect(m_toolbar, &OverlayToolbar::ocrRequested, this, &OverlayController::requestOcr);
        connect(m_toolbar, &OverlayToolbar::cancelRequested, this, &OverlayController::cancel);
        connect(m_toolbar, &OverlayToolbar::undoRequested, this, [this]() {
            finishPendingTextInput();
            if (m_editor != nullptr) {
                m_editor->undo();
            }
        });
        connect(m_toolbar, &OverlayToolbar::redoRequested, this, [this]() {
            finishPendingTextInput();
            if (m_editor != nullptr) {
                m_editor->redo();
            }
        });
        connect(m_toolbar, &OverlayToolbar::colorChangeRequested, this, [this](const QColor& color) {
            finishPendingTextInput();
            m_style.color = color;
            if (m_editor != nullptr) {
                m_editor->setStyle(m_style);
            }
        });
        connect(m_toolbar, &OverlayToolbar::widthChangeRequested, this, [this](int width) {
            finishPendingTextInput();
            m_style.width = width;
            if (m_editor != nullptr) {
                m_editor->setStyle(m_style);
            }
        });
        connect(m_toolbar, &OverlayToolbar::fillToggled, this, [this](bool filled) {
            finishPendingTextInput();
            m_style.filled = filled;
            if (m_editor != nullptr) {
                m_editor->setStyle(m_style);
            }
        });
        connect(m_toolbar, &OverlayToolbar::dashToggled, this, [this](bool dashed) {
            finishPendingTextInput();
            m_style.dashed = dashed;
            if (m_editor != nullptr) {
                m_editor->setStyle(m_style);
            }
        });
        m_toolbar->setCurrentColor(m_style.color);
        m_toolbar->setCurrentWidth(m_style.width);
    }
    m_toolbar->show();
    m_toolbar->raise();
}

void OverlayController::raiseToolbar()
{
    if (m_toolbar != nullptr && m_toolbar->isVisible()) {
        m_toolbar->raise();
    }
}

void OverlayController::hideToolbar()
{
    if (m_toolbar != nullptr) {
        m_toolbar->hide();
    }
}

void OverlayController::placeToolbar()
{
    if (m_toolbar == nullptr) {
        return;
    }
    if (!m_selection.hasUsableSelection()) {
        hideToolbar();
        return;
    }
    m_toolbar->positionNear(selectionPhysical(), selectionGlobalLogical(), m_virtualLogicalDesktop);
    m_toolbar->raise();
}

QRect OverlayController::selectionPhysical() const
{
    return m_selection.rect();
}

QRect OverlayController::selectionGlobalLogical() const
{
    const QRect physical = m_selection.rect();
    if (physical.isEmpty() || m_monitors.isEmpty()) {
        return {};
    }
    const QRectF globalRect = capture::physicalRectToGlobalLogical(m_monitors, QRectF(physical));
    return globalRect.toAlignedRect();
}

void OverlayController::onSelectionChanged()
{
    for (OverlayWindow* window : m_overlays) {
        window->refresh();
    }
    if (!m_ocrMode) {
        showToolbar();
        placeToolbar();
    }
}

void OverlayController::onSelectionReleased()
{
    if (!m_selection.hasUsableSelection()) {
        hideToolbar();
        return;
    }

    if (m_ocrMode) {
        requestOcr();
        return;
    }

    showToolbar();
    placeToolbar();
    focusOverlay();
    raiseToolbar();
}

// 文字输入框是编辑器内部的子控件，点工具条不会让它失焦（工具条不接收焦点），
// 因此任何工具条动作前都要显式结束输入：有内容提交，没内容丢弃。
void OverlayController::finishPendingTextInput()
{
    if (m_editor != nullptr && m_editor->canvas() != nullptr) {
        m_editor->canvas()->commitOrDiscardTextInput();
    }
}

void OverlayController::enterEditingMode(annotate::AnnotationKind kind)
{
    finishPendingTextInput();
    if (!m_selection.hasUsableSelection()) {
        return;
    }

    // 已在编辑态：只切换工具，绝不能重新 setBaseImage ——
    // AnnotationDocument::setBaseImage() 会 clear() 图元，曾导致
    // “画完矩形再点椭圆，矩形被删掉”。
    if (m_editor != nullptr) {
        m_editor->setTool(kind);
        m_toolbar->setEditingMode(true);
        m_toolbar->setCurrentTool(kind);
        if (m_editor->canvas() != nullptr) {
            m_toolbar->setUndoRedoEnabled(m_editor->canvas()->canUndo(),
                                          m_editor->canvas()->canRedo());
        }
        m_editor->raise();
        raiseToolbar();
        return;
    }

    const QImage cropped = croppedSelection();
    if (cropped.isNull()) {

        emit errorOccurred(QStringLiteral("选区图像为空，无法进入标注"));
        return;
    }

    m_document.setBaseImage(cropped);

    if (m_editor == nullptr) {
        m_editor = new editor::EditorWindow;
        m_editor->setStyle(m_style);
        connect(m_editor, &editor::EditorWindow::confirmRequested, this,
                &OverlayController::requestCopy);
        connect(m_editor, &editor::EditorWindow::cancelRequested, this,
                &OverlayController::exitEditingMode);
        connect(m_editor, &editor::EditorWindow::saveRequested, this,
                &OverlayController::requestSave);
        m_editor->setDocument(&m_document);
        if (m_editor->canvas() != nullptr) {
            connect(m_editor->canvas(), &editor::AnnotationCanvas::undoStateChanged, this,
                    [this]() {
                        if (m_toolbar != nullptr && m_editor != nullptr
                            && m_editor->canvas() != nullptr) {
                            m_toolbar->setUndoRedoEnabled(m_editor->canvas()->canUndo(),
                                                          m_editor->canvas()->canRedo());
                        }
                    });
        }
    }

    m_editor->placeAt(selectionGlobalLogical());
    m_editor->show();
    m_editor->raise();
    m_editor->activateWindow();
    m_editor->setTool(kind);

    // 编辑态保留覆盖层：选区外的暗化背景必须继续显示（用户要求“标注时周围仍是灰的”）。
    // 只把选区交互关掉（避免误拖动选区），并把编辑窗提到覆盖层之上。
    for (OverlayWindow* window : m_overlays) {
        window->setSelectionInteractive(false);
        window->refresh();
    }

    showToolbar();
    m_toolbar->setEditingMode(true);
    m_toolbar->setCurrentTool(kind);
    if (m_editor->canvas() != nullptr) {
        m_toolbar->setUndoRedoEnabled(m_editor->canvas()->canUndo(), m_editor->canvas()->canRedo());
    }
    placeToolbar();
    // 覆盖层是置顶全屏窗口，编辑窗必须显式提到它们之上。
    for (OverlayWindow* window : m_overlays) {
        window->raise();
    }
    m_toolbar->raise();
    m_editor->raise();
    // 延迟一拍再激活：此刻仍在工具条点击的事件处理中，立即 SetForegroundWindow 会被系统忽略，
    // 导致用户“第一次点击”被当作激活点击吞掉。
    QTimer::singleShot(0, m_editor, [this]() {
        if (m_editor == nullptr) {
            return;
        }
        platform::win32::forceForegroundWindow(m_editor);
        if (m_editor->canvas() != nullptr) {
            m_editor->canvas()->setFocus();
        } else {
            m_editor->setFocus();
        }
        // 激活编辑窗会把它带到最前，但覆盖层/工具条也要保持正确层级。
        for (OverlayWindow* window : m_overlays) {
            window->raise();
        }
        raiseToolbar();
        m_editor->raise();
    });
}

void OverlayController::exitEditingMode()
{
    if (m_editor != nullptr) {
        m_editor->hide();
        m_editor->deleteLater();
        m_editor = nullptr;
    }
    m_document.clearItems();
    for (OverlayWindow* window : m_overlays) {
        window->setSelectionInteractive(true);
        window->show();
        window->refresh();
    }
    if (m_toolbar != nullptr) {
        m_toolbar->setEditingMode(false);
    }
    showToolbar();
    placeToolbar();
    focusOverlay();
    raiseToolbar();
}

QImage OverlayController::croppedSelection() const
{
    const QRect physical = m_selection.rect();
    if (physical.isEmpty() || !m_result.isValid()) {
        return {};
    }
    return capture::cropSelection(m_result, physical);
}

QImage OverlayController::currentImage() const
{
    if (m_editor != nullptr) {
        const QImage rendered = m_editor->render();
        if (!rendered.isNull()) {
            return rendered;
        }
    }
    return croppedSelection();
}

void OverlayController::requestCopy()
{
    finishPendingTextInput();
    const QImage image = currentImage();
    if (!image.isNull()) {
        emit copyRequested(image);
    }
    finish();
}

bool OverlayController::lastSelectionTouchesScreenEdge() const
{
    if (m_virtualLogicalDesktop.isEmpty()) {
        return false;
    }
    const QRect selection = m_selection.rect();
    if (selection.isEmpty()) {
        return false;
    }
    // 选区与虚拟桌面都用逻辑坐标，直接比较边界即可
    return ocr::selectionTouchesScreenEdge(selection, m_virtualLogicalDesktop);
}

void OverlayController::handleDragOut(const QRect& physicalSelection)
{
    if (physicalSelection.isEmpty()) {
        return;
    }
    const QImage image = capture::cropSelection(m_result, physicalSelection);
    if (image.isNull()) {
        return;
    }
    ui::startImageDrag(m_overlays.isEmpty() ? nullptr : m_overlays.first(), image);
}

void OverlayController::requestScrolling()
{
    finishPendingTextInput();
    const QRect selection = m_selection.rect();
    if (selection.isEmpty()) {
        return;
    }
    emit scrollingRequested(selection);
    finish();
}

void OverlayController::requestPin()
{
    finishPendingTextInput();
    const QImage image = currentImage();
    if (!image.isNull()) {
        emit pinRequested(image);
    }
    finish();
}

void OverlayController::requestSave()
{
    finishPendingTextInput();
    const QImage image = currentImage();
    if (!image.isNull()) {
        emit saveRequested(image);
    }
    finish();
}

void OverlayController::requestOcr()
{
    finishPendingTextInput();
    const QImage image = croppedSelection();
    if (!image.isNull()) {
        emit ocrRequested(image);
    }
    finish();
}

void OverlayController::focusOverlay()
{
    if (m_overlays.isEmpty()) {
        return;
    }
    // 优先聚焦选区所在显示器，否则第一块屏。
    OverlayWindow* target = m_overlays.first();
    const QPoint selectionCenter = m_selection.rect().center();
    for (OverlayWindow* window : m_overlays) {
        if (window->monitor().physicalGeometry.contains(selectionCenter)) {
            target = window;
            break;
        }
    }
    QTimer::singleShot(0, target, [this, target]() {
        platform::win32::forceForegroundWindow(target);
        target->setFocus();
        // 前台化会把覆盖层提到最上，必须再次抬升工具条，否则工具条被压住点不到。
        raiseToolbar();
    });
}

void OverlayController::finish()
{
    stopMagnifierPolling();
    if (m_editor != nullptr) {
        m_editor->hide();
        m_editor->deleteLater();
        m_editor = nullptr;
    }
    hideToolbar();
    if (m_toolbar != nullptr) {
        m_toolbar->setEditingMode(false);
        m_toolbar->deleteLater();
        m_toolbar = nullptr;
    }
    destroyOverlays();
    m_selection.reset();
    m_result = capture::CaptureResult{};
    m_document.clearItems();
    m_active = false;
    m_ocrMode = false;
    emit finished();
}

}  // namespace snapocr::overlay
