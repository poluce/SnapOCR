#include "overlay/OverlayToolbar.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QEvent>
#include <QFrame>
#include <QHelpEvent>
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
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
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

    auto* selectButton = makeButton(ui::actionIcon(QStringLiteral("select"), kToolbarInk), QStringLiteral("选择/移动图元"));
    toolLayout->addWidget(selectButton);
    connect(selectButton, &QToolButton::clicked, this, [this]() { emit selectToolSelected(); });

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
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
    auto* undoButton = makeButton(ui::actionIcon(QStringLiteral("undo"), kToolbarInk), QStringLiteral("撤销 Ctrl+Z"));
    auto* redoButton = makeButton(ui::actionIcon(QStringLiteral("redo"), kToolbarInk), QStringLiteral("重做 Ctrl+Y"));
    undoLayout->addWidget(undoButton);
    undoLayout->addWidget(redoButton);
    connect(undoButton, &QToolButton::clicked, this, [this]() { emit undoRequested(); });
    connect(redoButton, &QToolButton::clicked, this, [this]() { emit redoRequested(); });

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
    auto* cancelButton = makeButton(ui::actionIcon(QStringLiteral("close"), kToolbarInk), QStringLiteral("取消 Esc"));
    cancelButton->setObjectName(QStringLiteral("cancelButton"));
    connect(ocrButton, &QToolButton::clicked, this, [this]() { emit ocrRequested(); });
    connect(saveButton, &QToolButton::clicked, this, [this]() { emit saveRequested(); });
    connect(copyButton, &QToolButton::clicked, this, [this]() { emit copyRequested(); });
    connect(cancelButton, &QToolButton::clicked, this, [this]() { emit cancelRequested(); });

    // 单行排列：工具 | 撤销重做 | 颜色线宽 | 动作
    rootLayout->addWidget(m_toolGroup);
    rootLayout->addWidget(makeSeparator());
    rootLayout->addWidget(m_undoGroup);
    rootLayout->addWidget(m_styleGroup);
    rootLayout->addWidget(makeSeparator());
    rootLayout->addWidget(ocrButton);
    rootLayout->addWidget(saveButton);
    rootLayout->addWidget(cancelButton);
    rootLayout->addWidget(copyButton);  // 勾号固定在最右侧

    setEditingMode(false);
    setCurrentColor(QColor(QStringLiteral("#FF3B30")));
    setCurrentWidth(3);
    applyStyleSheet();
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
    // 工具与动作按钮始终可见（单行）；撤销/颜色线宽仅在编辑模式出现。
    m_toolGroup->setVisible(true);
    m_styleGroup->setVisible(editing);
    m_undoGroup->setVisible(editing);
    adjustSize();
}

void OverlayToolbar::setCurrentTool(annotate::AnnotationKind kind)
{
    // 与 kTools 顺序对应：下标 0 是“选择/移动”，不作为 checkable 按钮管理。
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
    QPixmap pixmap(18, 18);
    pixmap.fill(color);
    QPainter painter(&pixmap);
    painter.setPen(QPen(QColor(0x66, 0x6B, 0x73), 1));
    painter.drawRect(0, 0, 17, 17);
    painter.end();
    m_colorButton->setIcon(QIcon(pixmap));
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
