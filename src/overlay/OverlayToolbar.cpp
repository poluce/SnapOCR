#include "overlay/OverlayToolbar.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

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

const QColor kToolbarInk(0xE8, 0xE8, 0xE8);

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

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(6, 4, 6, 4);
    rootLayout->setSpacing(4);

    // ---- 第一行：标注工具（仅编辑模式显示） ----
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

    toolLayout->addSpacing(6);
    m_undoGroup = new QWidget(m_toolGroup);
    auto* undoLayout = new QHBoxLayout(m_undoGroup);
    undoLayout->setContentsMargins(0, 0, 0, 0);
    undoLayout->setSpacing(2);
    auto* undoButton = makeButton(ui::actionIcon(QStringLiteral("undo"), kToolbarInk), QStringLiteral("撤销 Ctrl+Z"));
    auto* redoButton = makeButton(ui::actionIcon(QStringLiteral("redo"), kToolbarInk), QStringLiteral("重做 Ctrl+Y"));
    undoLayout->addWidget(undoButton);
    undoLayout->addWidget(redoButton);
    toolLayout->addWidget(m_undoGroup);
    connect(undoButton, &QToolButton::clicked, this, [this]() { emit undoRequested(); });
    connect(redoButton, &QToolButton::clicked, this, [this]() { emit redoRequested(); });

    toolLayout->addSpacing(6);
    m_styleGroup = new QWidget(m_toolGroup);
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
    toolLayout->addWidget(m_styleGroup);

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

    rootLayout->addWidget(m_toolGroup);

    // ---- 第二行：动作按钮（始终显示） ----
    auto* actionGroup = new QWidget(this);
    auto* actionLayout = new QHBoxLayout(actionGroup);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(2);

    auto* ocrButton = makeButton(ui::actionIcon(QStringLiteral("ocr"), kToolbarInk), QStringLiteral("提取文字（F4 直接取字）"));
    auto* saveButton = makeButton(ui::actionIcon(QStringLiteral("save"), kToolbarInk), QStringLiteral("保存 Ctrl+S"));
    auto* copyButton = makeButton(ui::actionIcon(QStringLiteral("copy"), kToolbarInk), QStringLiteral("复制到剪贴板 Enter"));
    auto* cancelButton = makeButton(ui::actionIcon(QStringLiteral("close"), kToolbarInk), QStringLiteral("取消 Esc"));
    actionLayout->addStretch(1);
    actionLayout->addWidget(ocrButton);
    actionLayout->addWidget(saveButton);
    actionLayout->addWidget(copyButton);
    actionLayout->addSpacing(6);
    actionLayout->addWidget(cancelButton);
    connect(ocrButton, &QToolButton::clicked, this, [this]() { emit ocrRequested(); });
    connect(saveButton, &QToolButton::clicked, this, [this]() { emit saveRequested(); });
    connect(copyButton, &QToolButton::clicked, this, [this]() { emit copyRequested(); });
    connect(cancelButton, &QToolButton::clicked, this, [this]() { emit cancelRequested(); });

    rootLayout->addWidget(actionGroup);

    setEditingMode(false);
    setCurrentColor(QColor(QStringLiteral("#FF3B30")));
    setCurrentWidth(3);
    applyStyleSheet();
}

void OverlayToolbar::applyStyleSheet()
{
    setStyleSheet(QStringLiteral(
        "QWidget#snapocrToolbar { background-color: #2B2B2B; border: 1px solid #555555;"
        " border-radius: 6px; }"
        "QToolButton { border: none; border-radius: 4px; color: #E8E8E8; }"
        "QToolButton:hover { background-color: #3D3D3D; }"
        "QToolButton:checked { background-color: #1F6FEB; }"
        "QToolButton:pressed { background-color: #16407A; }"));
}

void OverlayToolbar::setEditingMode(bool editing)
{
    m_editing = editing;
    // 标注工具行始终可见：选择模式下点击任一工具即进入编辑模式。
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
    painter.setPen(QPen(QColor(0xEE, 0xEE, 0xEE), 1));
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
