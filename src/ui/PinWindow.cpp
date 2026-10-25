#include "ui/PinWindow.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QTransform>
#include <QWheelEvent>
#include <QWindow>

#include "platform/win32/WindowActivation.h"
#include "ui/ImageDrag.h"

namespace snapocr::ui {
namespace {

constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 4.0;

}  // namespace

PinWindow::PinWindow(const QImage& image, QWidget* parent)
    : QWidget(parent)
    , m_image(image)
{
    // 置顶无边框工具窗口：不抢任务栏，但需要键盘焦点（Esc/R/Ctrl+C）。
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("SnapOCR 贴图"));
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
    setMouseTracking(true);

    // 截图像素是物理像素；窗口尺寸是逻辑像素。200% 屏上 1201×801 的图
    // 必须是 600×400 逻辑窗口，否则贴图会超出屏幕一倍。
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        m_devicePixelRatio = qMax(1.0, screen->devicePixelRatio());
    }

    updateWindowSize();

    // 显示后主动拿一次前台焦点，否则 Esc 等按键收不到。
    QTimer::singleShot(0, this, [this]() {
        show();
        platform::win32::forceForegroundWindow(this);
        setFocus(Qt::ActiveWindowFocusReason);
    });
}

void PinWindow::updateWindowSize()
{
    QImage rotated = m_image;
    if (m_rotation % 360 != 0) {
        QTransform transform;
        transform.rotate(m_rotation % 360);
        rotated = m_image.transformed(transform, Qt::SmoothTransformation);
    }
    const QSize size(qMax(1, qRound(rotated.width() * m_zoom / m_devicePixelRatio)),
                     qMax(1, qRound(rotated.height() * m_zoom / m_devicePixelRatio)));
    resize(size);
}

void PinWindow::setZoom(qreal zoom)
{
    m_zoom = qBound(kMinZoom, zoom, kMaxZoom);
    updateWindowSize();
    update();
}

void PinWindow::applyZoom(qreal zoom, const QPoint& anchorLocal)
{
    const qreal oldZoom = m_zoom;
    const qreal next = qBound(kMinZoom, zoom, kMaxZoom);
    if (qFuzzyCompare(oldZoom, next)) {
        return;
    }
    // 以光标为锚点：缩放后锚点下方的图像内容保持不动。
    const QPoint globalAnchor = mapToGlobal(anchorLocal);
    const qreal ratio = next / oldZoom;
    const QPoint offset = anchorLocal - QPoint(0, 0);
    m_zoom = next;
    updateWindowSize();
    const QPoint newLocal = QPoint(qRound(offset.x() * ratio), qRound(offset.y() * ratio));
    move(globalAnchor - newLocal);
}

void PinWindow::rotateBy90()
{
    m_rotation = (m_rotation + 90) % 360;
    updateWindowSize();
    update();
}

void PinWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QImage source = m_image;
    if (m_rotation % 360 != 0) {
        QTransform transform;
        transform.rotate(m_rotation % 360);
        source = m_image.transformed(transform, Qt::SmoothTransformation);
    }
    // zoom*dpr == 1 时按“一物理像素对应一设备像素”绘制，避免缩放发虚。
    const bool exactPixels = qFuzzyCompare(m_zoom, m_devicePixelRatio);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, !exactPixels);
    painter.drawImage(rect(), source);

    // 1px 边框，便于在浅色背景上分辨贴图边界
    painter.setPen(QPen(QColor(0x1F, 0x6F, 0xEB, 180), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void PinWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    // F-SYS-12：Ctrl+左键把贴图拖到资源管理器/IM；普通左键仍是移动窗口
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        ui::startImageDrag(this, m_image);
        event->accept();
        return;
    }
    if (QWindow* handle = windowHandle()) {
        // 交给系统拖动：高 DPI / 多屏下不会跟手偏移。
        handle->startSystemMove();
    }
    m_dragging = true;
    m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void PinWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        // startSystemMove 生效时系统接管移动，这里只做兜底。
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void PinWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PinWindow::wheelEvent(QWheelEvent* event)
{
    const int steps = event->angleDelta().y();
    if (steps == 0) {
        return;
    }
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        // Ctrl+滚轮：调透明度
        m_opacityPercent = qBound(20, m_opacityPercent + (steps > 0 ? 10 : -10), 100);
        setWindowOpacity(m_opacityPercent / 100.0);
    } else {
        const qreal factor = steps > 0 ? 1.1 : (1.0 / 1.1);
        applyZoom(m_zoom * factor, event->position().toPoint());
    }
    event->accept();
}

void PinWindow::requestOcr()
{
    if (!m_image.isNull()) {
        emit ocrRequested(m_image);
    }
}

void PinWindow::requestEdit()
{
    if (!m_image.isNull()) {
        emit editRequested(m_image);
    }
}

void PinWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        close();
        return;
    case Qt::Key_R:
        rotateBy90();
        return;
    case Qt::Key_T:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            requestOcr();
            return;
        }
        break;
    case Qt::Key_E:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            requestEdit();
            return;
        }
        break;
    case Qt::Key_C:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            emit copyRequested(m_image);
            return;
        }
        break;
    case Qt::Key_S:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            emit saveRequested(m_image);
            return;
        }
        break;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void PinWindow::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* ocrAction = menu.addAction(QStringLiteral("提取文字 (Ctrl+T)"));
    QAction* editAction = menu.addAction(QStringLiteral("在编辑器中打开 (Ctrl+E)"));
    menu.addSeparator();
    QAction* copyAction = menu.addAction(QStringLiteral("复制到剪贴板 (Ctrl+C)"));
    QAction* saveAction = menu.addAction(QStringLiteral("保存 (Ctrl+S)"));
    menu.addSeparator();
    QAction* zoomInAction = menu.addAction(QStringLiteral("放大"));
    QAction* zoomOutAction = menu.addAction(QStringLiteral("缩小"));
    QAction* rotateAction = menu.addAction(QStringLiteral("旋转 90° (R)"));
    QAction* resetAction = menu.addAction(QStringLiteral("恢复 100%"));
    menu.addSeparator();
    QAction* closeAction = menu.addAction(QStringLiteral("关闭 (Esc)"));

    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == nullptr) {
        return;
    }
    if (chosen == ocrAction) {
        requestOcr();
    } else if (chosen == editAction) {
        requestEdit();
    } else if (chosen == copyAction) {
        emit copyRequested(m_image);
    } else if (chosen == saveAction) {
        emit saveRequested(m_image);
    } else if (chosen == zoomInAction) {
        applyZoom(m_zoom * 1.1, rect().center());
    } else if (chosen == zoomOutAction) {
        applyZoom(m_zoom / 1.1, rect().center());
    } else if (chosen == rotateAction) {
        rotateBy90();
    } else if (chosen == resetAction) {
        setZoom(1.0);
        setWindowOpacity(1.0);
        m_opacityPercent = 100;
    } else if (chosen == closeAction) {
        close();
    }
}

}  // namespace snapocr::ui
