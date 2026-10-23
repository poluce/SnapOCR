#include "overlay/OverlayWindow.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QWindow>

#include "capture/CoordinateMap.h"

namespace snapocr::overlay {
namespace {

constexpr int kHandleSizeLogical = 7;
constexpr qreal kDimAlpha = 110.0;

QColor accentColor()
{
    return QColor(0x1F, 0x8F, 0xEB);
}

}  // namespace

OverlayWindow::OverlayWindow(const capture::ScreenFrame& frame,
                             const QVector<capture::MonitorInfo>* monitors,
                             SelectionModel* selection, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_frame(frame)
    , m_monitors(monitors)
    , m_selection(selection)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setWindowTitle(QStringLiteral("SnapOCR Overlay"));

    // 覆盖到该显示器：逻辑几何来自 Qt，设备像素比由窗口所在屏幕决定。
    setGeometry(m_frame.monitor.logicalGeometry);
}

void OverlayWindow::setSelectionInteractive(bool interactive)
{
    m_interactive = interactive;
    setCursor(interactive ? Qt::CrossCursor : Qt::ArrowCursor);
}

void OverlayWindow::refresh()
{
    update();
}

QPoint OverlayWindow::physicalFromGlobal(const QPointF& globalLogical) const
{
    if (m_monitors == nullptr) {
        return globalLogical.toPoint();
    }
    return capture::globalLogicalToPhysicalPoint(*m_monitors, globalLogical);
}

QRectF OverlayWindow::selectionInLocal() const
{
    if (m_selection == nullptr || !m_selection->isActive()) {
        return {};
    }
    const QRect physical = m_selection->rect();
    if (physical.isEmpty()) {
        return {};
    }
    return capture::physicalRectToLocalLogical(m_frame.monitor, QRectF(physical));
}

void OverlayWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const QRectF target(QPointF(0, 0), QSizeF(size()));

    // 1. 冻结画面（按物理像素 1:1 绘制到整块屏）
    painter.drawImage(target, m_frame.image);

    // 2. 暗化遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, static_cast<int>(kDimAlpha)));

    const QRectF selectionLocal = selectionInLocal();
    if (selectionLocal.isEmpty()) {
        return;
    }

    // 3. 选区透出：重绘未暗化的原始画面
    painter.save();
    painter.setClipRect(selectionLocal);
    painter.drawImage(target, m_frame.image);
    painter.restore();

    // 4. 边框与控制点
    QPen borderPen(accentColor());
    borderPen.setWidthF(1.5);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(selectionLocal.adjusted(-0.5, -0.5, 0.5, 0.5));

    if (m_interactive) {
        const qreal half = kHandleSizeLogical / 2.0;
        const QPointF corners[8] = {
            selectionLocal.topLeft(),
            QPointF(selectionLocal.center().x(), selectionLocal.top()),
            selectionLocal.topRight(),
            QPointF(selectionLocal.right(), selectionLocal.center().y()),
            selectionLocal.bottomRight(),
            QPointF(selectionLocal.center().x(), selectionLocal.bottom()),
            selectionLocal.bottomLeft(),
            QPointF(selectionLocal.left(), selectionLocal.center().y()),
        };
        painter.setPen(QPen(QColor(0xFF, 0xFF, 0xFF), 1));
        painter.setBrush(QBrush(accentColor()));
        for (const QPointF& corner : corners) {
            painter.drawRect(QRectF(corner.x() - half, corner.y() - half, kHandleSizeLogical,
                                    kHandleSizeLogical));
        }
    }

    // 5. 尺寸/坐标提示
    paintHud(painter, selectionLocal);
}

void OverlayWindow::paintHud(QPainter& painter, const QRectF& selectionLocal)
{
    const QRect physical = m_selection->rect();
    const QString text = QStringLiteral("%1 × %2  (%3, %4)")
                             .arg(physical.width())
                             .arg(physical.height())
                             .arg(physical.x())
                             .arg(physical.y());

    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);

    const QFontMetrics metrics(font);
    const int padding = 6;
    const QSize textSize = metrics.size(Qt::TextSingleLine, text);
    const QSize boxSize(textSize.width() + padding * 2, textSize.height() + padding);

    qreal x = selectionLocal.left();
    qreal y = selectionLocal.top() - boxSize.height() - 6;
    if (y < 0) {
        y = selectionLocal.top() + 6;
    }
    if (x + boxSize.width() > width()) {
        x = width() - boxSize.width() - 2;
    }

    const QRectF box(x, y, boxSize.width(), boxSize.height());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 170));
    painter.drawRoundedRect(box, 3, 3);
    painter.setPen(QPen(QColor(0xFF, 0xFF, 0xFF)));
    painter.drawText(box, Qt::AlignCenter, text);
}

void OverlayWindow::mousePressEvent(QMouseEvent* event)
{
    if (!m_interactive || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint physical = physicalFromGlobal(event->globalPosition());
    const int handleSize = qRound(kHandleSizeLogical * m_frame.monitor.devicePixelRatio);

    if (m_selection->isActive() && m_selection->hasUsableSelection()) {
        const SelectionModel::Handle handle = m_selection->hitTest(physical, handleSize);
        if (handle == SelectionModel::Handle::Inside) {
            m_selection->beginMove(physical);
            m_activeHandle = handle;
            m_dragging = true;
            return;
        }
        if (handle != SelectionModel::Handle::None) {
            m_selection->beginResize(handle, physical);
            m_activeHandle = handle;
            m_dragging = true;
            return;
        }
    }

    // 空白处重新框选
    m_selection->beginCreate(physical);
    m_activeHandle = SelectionModel::Handle::None;
    m_dragging = true;
    emit selectionChanged();
    update();
}

void OverlayWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_interactive) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint physical = physicalFromGlobal(event->globalPosition());

    if (m_dragging) {
        switch (m_selection->dragMode()) {
        case SelectionModel::DragMode::Creating:
            m_selection->updateCreate(physical);
            break;
        case SelectionModel::DragMode::Moving:
        case SelectionModel::DragMode::Resizing:
            m_selection->updateDrag(physical);
            break;
        case SelectionModel::DragMode::None:
            break;
        }
        emit selectionChanged();
        return;
    }

    // 悬停光标反馈
    if (m_selection->isActive() && m_selection->hasUsableSelection()) {
        const int handleSize = qRound(kHandleSizeLogical * m_frame.monitor.devicePixelRatio);
        switch (m_selection->hitTest(physical, handleSize)) {
        case SelectionModel::Handle::TopLeft:
        case SelectionModel::Handle::BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case SelectionModel::Handle::TopRight:
        case SelectionModel::Handle::BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case SelectionModel::Handle::Left:
        case SelectionModel::Handle::Right:
            setCursor(Qt::SizeHorCursor);
            break;
        case SelectionModel::Handle::Top:
        case SelectionModel::Handle::Bottom:
            setCursor(Qt::SizeVerCursor);
            break;
        case SelectionModel::Handle::Inside:
            setCursor(Qt::SizeAllCursor);
            break;
        case SelectionModel::Handle::None:
            setCursor(Qt::CrossCursor);
            break;
        }
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_interactive || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (!m_dragging) {
        return;
    }
    m_dragging = false;
    {
        const QPoint relPhysical = physicalFromGlobal(event->globalPosition());
        const QRect sel = m_selection->rect();
    }

    // 纯单击（几乎无移动）视为清空选区。
    const QRect current = m_selection->rect();
    if (m_activeHandle == SelectionModel::Handle::None && current.width() < 2 && current.height() < 2) {
        m_selection->reset();
        emit selectionChanged();
        return;
    }

    emit selectionChanged();
    if (m_selection->hasUsableSelection()) {
        emit selectionReleased();
    }
}

void OverlayWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!m_interactive || event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    if (m_selection->hasUsableSelection()) {
        emit selectionConfirmed();
    }
}

void OverlayWindow::keyPressEvent(QKeyEvent* event)
{
    if (!m_interactive) {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Escape:
        emit cancelled();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_selection->hasUsableSelection()) {
            emit selectionConfirmed();
        }
        return;
    case Qt::Key_S:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            emit saveRequested();
            return;
        }
        break;
    case Qt::Key_C:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            emit copyRequested();
            return;
        }
        break;
    case Qt::Key_A:
        if (event->modifiers().testFlag(Qt::ControlModifier) && m_monitors != nullptr) {
            QRect united;
            bool first = true;
            for (const capture::MonitorInfo& monitor : *m_monitors) {
                united = first ? monitor.physicalGeometry : united.united(monitor.physicalGeometry);
                first = false;
            }
            m_selection->setRect(united);
            emit selectionChanged();
            emit selectionReleased();
            return;
        }
        break;
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down: {
        const int step = event->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1;
        int dx = 0;
        int dy = 0;
        if (event->key() == Qt::Key_Left) {
            dx = -step;
        } else if (event->key() == Qt::Key_Right) {
            dx = step;
        } else if (event->key() == Qt::Key_Up) {
            dy = -step;
        } else {
            dy = step;
        }
        m_selection->nudge(dx, dy);
        emit selectionChanged();
        return;
    }
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

}  // namespace snapocr::overlay
