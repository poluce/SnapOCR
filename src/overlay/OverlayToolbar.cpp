#include "overlay/OverlayToolbar.h"

#include <QAbstractButton>
#include <QSignalBlocker>
#include <QButtonGroup>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QHelpEvent>
#include <QTimer>
#include <QLabel>
#include <QToolButton>

#include "overlay/ToolbarTooltip.h"
#include "ui/ToolIcons.h"

namespace snapocr::overlay {
namespace {

struct ToolDefinition
{
    annotate::AnnotationKind kind;
    const char* tip;
};

const ToolDefinition kTools[] = {
    {annotate::AnnotationKind::Rectangle, "矩形"},
    {annotate::AnnotationKind::Ellipse, "椭圆"},
    {annotate::AnnotationKind::Line, "直线"},
    {annotate::AnnotationKind::Arrow, "箭头"},
    {annotate::AnnotationKind::Freehand, "画笔"},
    {annotate::AnnotationKind::Text, "文字"},
    {annotate::AnnotationKind::StepNumber, "序号"},
    {annotate::AnnotationKind::Highlight, "高亮"},
    {annotate::AnnotationKind::Mosaic, "马赛克"},
};

// 工具条为浅色主题：深色描边图标。
const QColor kToolbarInk(0x2B, 0x2F, 0x36);

QToolButton* makeButton(const QIcon& icon, const QString& tip, const QString& objectName = {})
{
    auto* button = new QToolButton;
    button->setIcon(icon);
    button->setIconSize(QSize(20, 20));
    button->setToolTip(tip);
    button->setAutoRaise(true);
    button->setFixedSize(30, 28);
    if (!objectName.isEmpty()) {
        button->setObjectName(objectName);
    }
    return button;
}

}  // namespace

OverlayToolbar::OverlayToolbar(QWidget* parent)
    : QWidget(parent)
{
    // WindowDoesNotAcceptFocus：点击工具条按钮不能让工具条抢走键盘焦点，
    // 否则编辑窗/覆盖层失焦，Enter、Ctrl+Z 等快捷键会失效。
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setObjectName(QStringLiteral("snapocrToolbar"));

    // 单行布局：工具 | 撤销重做 | 颜色线宽 | 动作（取字 / 保存 / 确认 / 关闭）
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(6, 4, 6, 4);
    rootLayout->setSpacing(2);

    auto makeSeparator = [this]() {
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::VLine);
        line->setFixedWidth(1);
        line->setStyleSheet(QStringLiteral("color: #D5D9E0;"));
        return line;
    };

    // ---- 标注工具组（选择 + 9 类工具；始终显示） ----
    m_toolGroup = new QWidget(this);
    auto* toolLayout = new QHBoxLayout(m_toolGroup);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(2);

    m_selectButton = makeButton(ui::actionIcon(QStringLiteral("select"), kToolbarInk),
                                QStringLiteral("选择/移动图元"));
    m_selectButton->setCheckable(true);
    m_selectButton->setObjectName(QStringLiteral("selectButton"));
    toolLayout->addWidget(m_selectButton);

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    m_toolGroupBox = group;
    group->addButton(m_selectButton);
    connect(m_selectButton, &QToolButton::clicked, this, [this]() {
        m_selectDrawn = true;
        emit selectToolSelected();
    });
    for (const ToolDefinition& definition : kTools) {
        QToolButton* button =
            makeButton(ui::toolIcon(definition.kind, kToolbarInk), QString::fromUtf8(definition.tip));
        button->setCheckable(true);
        group->addButton(button);
        toolLayout->addWidget(button);
        m_toolButtons.append(button);
        const annotate::AnnotationKind kind = definition.kind;
        connect(button, &QToolButton::clicked, this, [this, kind]() { emit toolSelected(kind); });
    }

    // ---- 撤销 / 重做（仅编辑模式） ----
    m_undoGroup = new QWidget(this);
    auto* undoLayout = new QHBoxLayout(m_undoGroup);
    undoLayout->setContentsMargins(0, 0, 0, 0);
    undoLayout->setSpacing(2);
    m_undoButton = makeButton(ui::actionIcon(QStringLiteral("undo"), kToolbarInk), QStringLiteral("撤销 Ctrl+Z"));
    m_redoButton = makeButton(ui::actionIcon(QStringLiteral("redo"), kToolbarInk), QStringLiteral("重做 Ctrl+Y"));
    m_undoButton->setEnabled(false);
    m_redoButton->setEnabled(false);
    undoLayout->addWidget(m_undoButton);
    undoLayout->addWidget(m_redoButton);
    connect(m_undoButton, &QToolButton::clicked, this, [this]() { emit undoRequested(); });
    connect(m_redoButton, &QToolButton::clicked, this, [this]() { emit redoRequested(); });

    // ---- 颜色 / 线宽（仅编辑模式） ----
    m_styleGroup = new QWidget(this);
    auto* styleLayout = new QHBoxLayout(m_styleGroup);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->setSpacing(2);
    m_colorButton = makeButton(QIcon(), QStringLiteral("颜色"));
    m_colorButton->setFixedSize(28, 28);
    m_widthButton = makeButton(QIcon(), QStringLiteral("线宽"));
    m_widthButton->setText(QStringLiteral("3"));
    m_widthButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_widthButton->setFixedSize(28, 28);
    styleLayout->addWidget(m_colorButton);
    styleLayout->addWidget(m_widthButton);

    connect(m_colorButton, &QToolButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_colorButton->property("color").value<QColor>(),
                                                     this, QStringLiteral("选择标注颜色"));
        if (chosen.isValid()) {
            setCurrentColor(chosen);
            emit colorChangeRequested(chosen);
        }
    });
    connect(m_widthButton, &QToolButton::clicked, this, [this]() {
        bool ok = false;
        const int width = QInputDialog::getInt(this, QStringLiteral("线宽"),
                                               QStringLiteral("描边宽度（像素）："),
                                               m_widthButton->text().toInt(), 1, 40, 1, &ok);
        if (ok) {
            setCurrentWidth(width);
            emit widthChangeRequested(width);
        }
    });

    // ---- 动作按钮（始终显示，排在最后） ----
    // 复制用勾号（确认语义），取消用叉号（关闭语义）。
    auto* ocrButton = makeButton(ui::actionIcon(QStringLiteral("ocr"), kToolbarInk), QStringLiteral("提取文字（F4 直接取字）"));
    auto* saveButton = makeButton(ui::actionIcon(QStringLiteral("save"), kToolbarInk), QStringLiteral("保存 Ctrl+S"));
    auto* copyButton = makeButton(ui::actionIcon(QStringLiteral("confirm"), kToolbarInk), QStringLiteral("复制到剪贴板 Enter"));
    copyButton->setObjectName(QStringLiteral("copyButton"));
    auto* pinButton = makeButton(ui::actionIcon(QStringLiteral("pin"), kToolbarInk),
                                 QStringLiteral("贴图到屏幕（置顶显示）"));
    pinButton->setObjectName(QStringLiteral("pinButton"));
    auto* cancelButton = makeButton(ui::actionIcon(QStringLiteral("close"), kToolbarInk), QStringLiteral("取消 Esc"));
    cancelButton->setObjectName(QStringLiteral("cancelButton"));
    connect(ocrButton, &QToolButton::clicked, this, [this]() { emit ocrRequested(); });
    connect(saveButton, &QToolButton::clicked, this, [this]() { emit saveRequested(); });
    connect(copyButton, &QToolButton::clicked, this, [this]() { emit copyRequested(); });
    connect(pinButton, &QToolButton::clicked, this, [this]() { emit pinRequested(); });
    connect(cancelButton, &QToolButton::clicked, this, [this]() { emit cancelRequested(); });

    // 单行排列：工具 | 撤销重做 | 颜色线宽 | 动作
    rootLayout->addWidget(m_toolGroup);
    rootLayout->addWidget(makeSeparator());
    rootLayout->addWidget(m_undoGroup);
    rootLayout->addWidget(m_styleGroup);
    rootLayout->addWidget(makeSeparator());
    rootLayout->addWidget(ocrButton);
    rootLayout->addWidget(saveButton);
    rootLayout->addWidget(pinButton);
    rootLayout->addWidget(cancelButton);
    rootLayout->addWidget(copyButton);  // 勾号固定在最右侧

    // 给每个按钮装事件过滤器：用 Enter/Leave + 定时器自己驱动提示，
    // 不依赖 Qt 的 QEvent::ToolTip（该机制在本应用里不会触发）。
    const auto allButtons = findChildren<QToolButton*>();
    for (QToolButton* button : allButtons) {
        button->installEventFilter(this);
    }

    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(500);
    connect(m_hoverTimer, &QTimer::timeout, this, [this]() {
        if (m_hoverButton == nullptr) {
            return;
        }
        // 鼠标可能已经离开：用光标位置兜底确认。
        const QPoint local = m_hoverButton->mapFromGlobal(QCursor::pos());
        if (!m_hoverButton->rect().contains(local)) {
            m_hoverButton = nullptr;
            return;
        }
        ToolbarTooltip::instance()->showFor(m_hoverButton, m_hoverButton->toolTip());
    });

    setEditingMode(false);
    setCurrentColor(QColor(QStringLiteral("#FF3B30")));
    setCurrentWidth(3);
    applyStyleSheet();
}

bool OverlayToolbar::eventFilter(QObject* watched, QEvent* event)
{
    auto* widget = qobject_cast<QWidget*>(watched);
    if (widget == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::Enter) {
        scheduleTooltip(widget);
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::Leave) {
        cancelTooltip();
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const QString text = widget->toolTip();
        if (!text.isEmpty()) {
            ToolbarTooltip::instance()->showFor(widget, text);
        } else {
            hideTooltipNow();
        }
        Q_UNUSED(help);
        return true;  // 阻止 Qt 默认提示
    }
    if (event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
        hideTooltipNow();
    }
    return QWidget::eventFilter(watched, event);
}

bool OverlayToolbar::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        QWidget* hit = childAt(help->pos());
        while (hit != nullptr && hit != this) {
            if (!hit->toolTip().isEmpty()) {
                ToolbarTooltip::instance()->showFor(hit, hit->toolTip());
                return true;  // 阻止 Qt 默认提示（会被全屏覆盖层盖住）
            }
            hit = hit->parentWidget();
        }
        hideTooltipNow();
        return true;
    }
    if (event->type() == QEvent::Leave || event->type() == QEvent::Hide
        || event->type() == QEvent::WindowDeactivate) {
        hideTooltipNow();
    }
    return QWidget::event(event);
}

void OverlayToolbar::hideTooltipNow()
{
    cancelTooltip();
    ToolbarTooltip::instance()->hideNow();
}

void OverlayToolbar::scheduleTooltip(QWidget* button)
{
    if (button == nullptr || button->toolTip().isEmpty()) {
        return;
    }
    ToolbarTooltip::instance()->hideNow();
    m_hoverButton = button;
    if (m_hoverTimer != nullptr) {
        m_hoverTimer->start();
    }
}

void OverlayToolbar::cancelTooltip()
{
    if (m_hoverTimer != nullptr) {
        m_hoverTimer->stop();
    }
    m_hoverButton = nullptr;
    ToolbarTooltip::instance()->hideNow();
}

void OverlayToolbar::applyStyleSheet()
{
    setStyleSheet(QStringLiteral(
        "QWidget#snapocrToolbar { background-color: #FAFBFC; border: 1px solid #C9CDD4;"
        " border-radius: 8px; }"
        "QToolButton { border: none; border-radius: 4px; color: #2B2F36; }"
        "QToolButton:hover { background-color: #E9EDF2; }"
        "QToolButton:checked { background-color: #CFE3FF; }"
        "QToolButton:pressed { background-color: #B9D4F7; }"));
}

void OverlayToolbar::setEditingMode(bool editing)
{
    m_editing = editing;
    // 标注工具行始终可见：选择模式下点击任一工具即进入编辑模式。
    // 整条工具条始终完整显示：工具 / 撤销重做 / 颜色线宽 / 动作按钮都不隐藏，
    // 避免切换编辑态时工具条宽度跳变、按钮位置移动（用户要求）。
    m_toolGroup->setVisible(true);
    m_styleGroup->setVisible(true);
    m_undoGroup->setVisible(true);
    if (!editing) {
        // 退出编辑后不能残留“某个工具被选中”的假状态，否则下次截图看起来像在编辑态。
        clearToolSelection();
    }
    adjustSize();
}

void OverlayToolbar::setSelectMode(bool select)
{
    m_selectDrawn = select;
    if (m_selectButton != nullptr) {
        m_selectButton->setChecked(select);
    }
    if (select) {
        // 互斥组：选中“选择”后不保留任何绘图工具的勾选态
        for (QToolButton* button : m_toolButtons) {
            button->setChecked(false);
        }
    }
    update();
}

void OverlayToolbar::clearToolSelection()
{
    m_selectDrawn = false;
    if (m_toolGroupBox != nullptr) {
        // 互斥组会强制保留一个选中项：必须先解除互斥，全部取消后再恢复。
        const bool wasExclusive = m_toolGroupBox->exclusive();
        m_toolGroupBox->setExclusive(false);
        const QSignalBlocker blocker(m_toolGroupBox);
        Q_UNUSED(blocker);
        for (QAbstractButton* button : m_toolGroupBox->buttons()) {
            button->setChecked(false);
        }
        m_toolGroupBox->setExclusive(wasExclusive);
    }
    update();
}

void OverlayToolbar::setUndoRedoEnabled(bool canUndo, bool canRedo)
{
    if (m_undoButton != nullptr) {
        m_undoButton->setEnabled(canUndo);
    }
    if (m_redoButton != nullptr) {
        m_redoButton->setEnabled(canRedo);
    }
}

void OverlayToolbar::setCurrentTool(annotate::AnnotationKind kind)
{
    // 与 kTools 顺序对应：下标 0 是“选择/移动”，不作为 checkable 按钮管理。
    if (m_selectButton != nullptr) {
        m_selectButton->setChecked(false);
    }
    for (int i = 0; i < m_toolButtons.size(); ++i) {
        if (kTools[i].kind == kind) {
            m_toolButtons[i]->setChecked(true);
            return;
        }
    }
}

void OverlayToolbar::setCurrentColor(const QColor& color)
{
    m_colorButton->setProperty("color", color);
    // 色块同样给多档 DPR，避免高 DPI 下发虚或取错尺寸。
    constexpr int kColorSize = 18;
    QIcon icon;
    for (int scale : {1, 2, 3, 4}) {
        QPixmap pixmap(kColorSize * scale, kColorSize * scale);
        pixmap.fill(color);
        QPainter painter(&pixmap);
        painter.scale(scale, scale);  // 按逻辑 18x18 空间绘制，边框才能铺满整张图
        painter.setPen(QPen(QColor(0x66, 0x6B, 0x73), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(0.5, 0.5, kColorSize - 1, kColorSize - 1));
        painter.end();
        icon.addPixmap(pixmap);
    }
    m_colorButton->setIcon(icon);
}

void OverlayToolbar::setCurrentWidth(int width)
{
    m_widthButton->setText(QString::number(width));
}

void OverlayToolbar::positionNear(const QRect& physicalSelection, const QRect& globalLogicalSelection,
                                  const QRect& virtualLogicalDesktop)
{
    Q_UNUSED(physicalSelection);
    adjustSize();
    const QSize toolbarSize = size();

    int x = globalLogicalSelection.left();
    int y = globalLogicalSelection.bottom() + 8;

    // 下方空间不足则放到选区上方；再不足则放进选区内部底部。
    if (y + toolbarSize.height() > virtualLogicalDesktop.bottom()) {
        y = globalLogicalSelection.top() - toolbarSize.height() - 8;
    }
    if (y < virtualLogicalDesktop.top()) {
        y = globalLogicalSelection.bottom() - toolbarSize.height() - 8;
    }

    // 右侧越界则右对齐。
    if (x + toolbarSize.width() > virtualLogicalDesktop.right()) {
        x = virtualLogicalDesktop.right() - toolbarSize.width() - 4;
    }
    if (x < virtualLogicalDesktop.left()) {
        x = virtualLogicalDesktop.left() + 4;
    }

    move(x, y);
}

}  // namespace snapocr::overlay
