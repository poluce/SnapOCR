#include "overlay/OverlayController.h"

#include <QApplication>
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

void OverlayController::beginCapture()
{
    if (m_active) {
        return;
    }
    m_ocrMode = false;
    m_document.clearItems();
    m_captureService->captureAllScreens();
}

void OverlayController::beginOcrCapture()
{
    if (m_active) {
        return;
    }
    m_ocrMode = true;
    m_document.clearItems();
    m_captureService->captureAllScreens();
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
    showToolbar();
    hideToolbar();  // 先隐藏，等选区出现再显示
    focusOverlay();
}

void OverlayController::buildOverlays()
{
    destroyOverlays();
    for (const capture::ScreenFrame& frame : m_result.screens) {
        auto* window = new OverlayWindow(frame, &m_monitors, &m_selection);
        connect(window, &OverlayWindow::selectionChanged, this,
                &OverlayController::onSelectionChanged);
        connect(window, &OverlayWindow::selectionReleased, this,
                &OverlayController::onSelectionReleased);
        connect(window, &OverlayWindow::selectionConfirmed, this,
                &OverlayController::requestCopy);
        connect(window, &OverlayWindow::saveRequested, this, &OverlayController::requestSave);
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
            if (m_editor != nullptr) {
                m_editor->setSelectMode(true);
            }
        });
        connect(m_toolbar, &OverlayToolbar::copyRequested, this, &OverlayController::requestCopy);
        connect(m_toolbar, &OverlayToolbar::saveRequested, this, &OverlayController::requestSave);
        connect(m_toolbar, &OverlayToolbar::ocrRequested, this, &OverlayController::requestOcr);
        connect(m_toolbar, &OverlayToolbar::cancelRequested, this, &OverlayController::cancel);
        connect(m_toolbar, &OverlayToolbar::undoRequested, this, [this]() {
            if (m_editor != nullptr) {
                m_editor->undo();
            }
        });
        connect(m_toolbar, &OverlayToolbar::redoRequested, this, [this]() {
            if (m_editor != nullptr) {
                m_editor->redo();
            }
        });
        connect(m_toolbar, &OverlayToolbar::colorChangeRequested, this, [this](const QColor& color) {
            m_style.color = color;
            if (m_editor != nullptr) {
                m_editor->setStyle(m_style);
            }
        });
        connect(m_toolbar, &OverlayToolbar::widthChangeRequested, this, [this](int width) {
            m_style.width = width;
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

void OverlayController::enterEditingMode(annotate::AnnotationKind kind)
{
    if (!m_selection.hasUsableSelection()) {
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
    }

    m_editor->placeAt(selectionGlobalLogical());
    m_editor->show();
    m_editor->raise();
    m_editor->activateWindow();
    m_editor->setTool(kind);

    // 编辑态隐藏覆盖层：置顶编辑窗与置顶覆盖层同级，同时存在会互相遮挡并吃掉鼠标/键盘。
    for (OverlayWindow* window : m_overlays) {
        window->setSelectionInteractive(false);
        window->hide();
    }

    showToolbar();
    m_toolbar->setEditingMode(true);
    m_toolbar->setCurrentTool(kind);
    placeToolbar();
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
        raiseToolbar();
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
        window->raise();
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
    const QImage image = currentImage();
    if (!image.isNull()) {
        emit copyRequested(image);
    }
    finish();
}

void OverlayController::requestSave()
{
    const QImage image = currentImage();
    if (!image.isNull()) {
        emit saveRequested(image);
    }
    finish();
}

void OverlayController::requestOcr()
{
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
