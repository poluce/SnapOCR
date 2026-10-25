#include <QtTest>

#include <QToolButton>
#include <QWidget>

#include "annotate/AnnotationTypes.h"
#include "overlay/OverlayToolbar.h"
#include "ui/ToolIcons.h"
#include "overlay/ToolbarTooltip.h"

#include <QHelpEvent>

using namespace snapocr;
using namespace snapocr::overlay;

// 工具条布局回归：必须是单行；选择模式下撤销/样式组隐藏；动作按钮齐全。
class OverlayToolbarTest final : public QObject
{
    Q_OBJECT

private slots:
    void selectionModeIsSingleRow();
    void editingModeIsStillSingleRow();
    void hasAllActionButtons();
    void copyUsesCheckAndCancelUsesX();
    void tooltipEventOpensCustomBubble();
    void selectModeChecksArrowOnly();
    void exitingEditingClearsToolSelection();
    void undoRedoButtonsReflectState();
    void savePreviewWhenRequested();

private:
    static int visibleRowCount(OverlayToolbar& toolbar);
};

int OverlayToolbarTest::visibleRowCount(OverlayToolbar& toolbar)
{
    // 统计可见按钮的纵向分组数：单行工具条里所有按钮的 y 应该基本一致。
    QSet<int> rows;
    const auto buttons = toolbar.findChildren<QToolButton*>();
    for (QToolButton* button : buttons) {
        if (!button->isVisible()) {
            continue;
        }
        rows.insert(button->mapTo(&toolbar, QPoint(0, 0)).y());
    }
    return rows.size();
}

void OverlayToolbarTest::selectionModeIsSingleRow()
{
    OverlayToolbar toolbar;
    toolbar.setEditingMode(false);
    toolbar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&toolbar));
    QCOMPARE(visibleRowCount(toolbar), 1);
    QVERIFY(toolbar.height() < 60);
}

void OverlayToolbarTest::editingModeIsStillSingleRow()
{
    OverlayToolbar editing;
    editing.setEditingMode(true);
    editing.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editing));
    QCOMPARE(visibleRowCount(editing), 1);
    QVERIFY(editing.height() < 60);

    // 撤销/重做、颜色/线宽始终显示，两种模式尺寸必须一致（切换时不跳动）。
    OverlayToolbar selecting;
    selecting.setEditingMode(false);
    selecting.show();
    QVERIFY(QTest::qWaitForWindowExposed(&selecting));
    QCOMPARE(visibleRowCount(selecting), 1);
    QCOMPARE(selecting.width(), editing.width());
    QCOMPARE(selecting.height(), editing.height());
}

void OverlayToolbarTest::hasAllActionButtons()
{
    OverlayToolbar toolbar;
    const auto buttons = toolbar.findChildren<QToolButton*>();
    // 1 选择 + 10 工具（含模糊笔刷）+ 2 撤销重做 + 4 颜色/线宽/填充/虚线
    // + 6 动作（取字/保存/长截图/贴图/取消/复制）= 23
    QCOMPARE(buttons.size(), 23);
    QVERIFY(toolbar.findChild<QToolButton*>(QStringLiteral("scrollingButton")) != nullptr);
    QVERIFY(toolbar.findChild<QToolButton*>(QStringLiteral("fillButton")) != nullptr);
    QVERIFY(toolbar.findChild<QToolButton*>(QStringLiteral("dashButton")) != nullptr);
    QVERIFY(toolbar.findChild<QToolButton*>(QStringLiteral("pinButton")) != nullptr);
    QVERIFY(!toolbar.findChild<QToolButton*>(QStringLiteral("copyButton"))->icon().isNull());
    QVERIFY(!toolbar.findChild<QToolButton*>(QStringLiteral("cancelButton"))->icon().isNull());
}

// 动作按钮的图标映射：复制必须是勾号（confirm），取消必须是叉号（close）。
// 直接与 ToolIcons 生成的同参数图标逐像素比较，避免靠形状启发式判断。
void OverlayToolbarTest::copyUsesCheckAndCancelUsesX()
{
    OverlayToolbar toolbar;
    auto* copy = toolbar.findChild<QToolButton*>(QStringLiteral("copyButton"));
    auto* cancel = toolbar.findChild<QToolButton*>(QStringLiteral("cancelButton"));
    QVERIFY(copy != nullptr);
    QVERIFY(cancel != nullptr);

    const QColor ink(0x2B, 0x2F, 0x36);  // 工具条浅色主题的图标颜色
    const QSize size(20, 20);
    const QImage copyImage = copy->icon().pixmap(size, 2.0).toImage();
    const QImage cancelImage = cancel->icon().pixmap(size, 2.0).toImage();
    const QImage confirmRef = ui::actionIcon(QStringLiteral("confirm"), ink).pixmap(size, 2.0).toImage();
    const QImage closeRef = ui::actionIcon(QStringLiteral("close"), ink).pixmap(size, 2.0).toImage();

    QVERIFY(!copyImage.isNull());
    QVERIFY(!cancelImage.isNull());
    QVERIFY2(copyImage == confirmRef, "copy button must use the check icon");
    QVERIFY2(cancelImage == closeRef, "cancel button must use the X icon");
    QVERIFY(copyImage != cancelImage);
}

// 选择模式：只有箭头按钮处于勾选态，绘图工具全部取消。
void OverlayToolbarTest::selectModeChecksArrowOnly()
{
    OverlayToolbar toolbar;
    toolbar.setEditingMode(true);
    toolbar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&toolbar));

    auto* select = toolbar.findChild<QToolButton*>(QStringLiteral("selectButton"));
    QVERIFY(select != nullptr);
    QVERIFY(select->isCheckable());

    toolbar.setCurrentTool(annotate::AnnotationKind::Rectangle);
    QVERIFY(select->isChecked() == false);

    toolbar.setSelectMode(true);
    QVERIFY(select->isChecked());
    // 绘图工具不应残留勾选
    int checkedTools = 0;
    const auto buttons = toolbar.findChildren<QToolButton*>();
    for (QToolButton* button : buttons) {
        if (button != select && button->isCheckable() && button->isChecked()) {
            ++checkedTools;
        }
    }
    QCOMPARE(checkedTools, 0);
}

// 退出编辑态后不能残留“某工具被选中”的假状态。
void OverlayToolbarTest::exitingEditingClearsToolSelection()
{
    OverlayToolbar toolbar;
    toolbar.setEditingMode(true);
    toolbar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&toolbar));

    toolbar.setCurrentTool(annotate::AnnotationKind::Ellipse);
    bool anyChecked = false;
    for (QToolButton* button : toolbar.findChildren<QToolButton*>()) {
        if (button->isCheckable() && button->isChecked()) {
            anyChecked = true;
        }
    }
    QVERIFY(anyChecked);

    toolbar.setEditingMode(false);
    for (QToolButton* button : toolbar.findChildren<QToolButton*>()) {
        if (button->isCheckable()) {
            QVERIFY2(!button->isChecked(), "tool must be unchecked after leaving edit mode");
        }
    }
}

// 撤销/重做按钮要反映可用状态，栈空时禁用。
void OverlayToolbarTest::undoRedoButtonsReflectState()
{
    OverlayToolbar toolbar;
    toolbar.setEditingMode(true);
    toolbar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&toolbar));

    const auto buttons = toolbar.findChildren<QToolButton*>();
    QToolButton* undo = nullptr;
    QToolButton* redo = nullptr;
    for (QToolButton* button : buttons) {
        if (button->toolTip().startsWith(QStringLiteral("撤销"))) {
            undo = button;
        } else if (button->toolTip().startsWith(QStringLiteral("重做"))) {
            redo = button;
        }
    }
    QVERIFY(undo != nullptr);
    QVERIFY(redo != nullptr);
    QVERIFY(!undo->isEnabled());
    QVERIFY(!redo->isEnabled());

    toolbar.setUndoRedoEnabled(true, false);
    QVERIFY(undo->isEnabled());
    QVERIFY(!redo->isEnabled());

    toolbar.setUndoRedoEnabled(false, true);
    QVERIFY(!undo->isEnabled());
    QVERIFY(redo->isEnabled());
}

// QEvent::ToolTip 只会发给鼠标下的按钮，工具条必须给每个按钮装过滤器接管，
// 否则 Qt 默认提示会被全屏置顶覆盖层压住（用户反馈“提示显示有问题”）。
void OverlayToolbarTest::tooltipEventOpensCustomBubble()
{
    OverlayToolbar toolbar;
    toolbar.setEditingMode(true);
    toolbar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&toolbar));

    const auto buttons = toolbar.findChildren<QToolButton*>();
    QVERIFY(buttons.size() >= 12);

    ToolbarTooltip* tooltip = ToolbarTooltip::instance();
    tooltip->hideNow();
    QVERIFY(!tooltip->isVisible());

    // 向第 2 个绘图工具按钮发送 ToolTip 事件
    QToolButton* target = nullptr;
    for (QToolButton* button : buttons) {
        if (button->toolTip() == QStringLiteral("矩形")) {
            target = button;
            break;
        }
    }
    QVERIFY2(target != nullptr, "rectangle button not found");

    const QPoint local(5, 5);
    QHelpEvent help(QEvent::ToolTip, local, target->mapToGlobal(local));
    QApplication::sendEvent(target, &help);

    QVERIFY2(help.isAccepted(), "tooltip event should be consumed by the toolbar filter");
    tooltip->hideNow();
}

// 仅当设置 SNAPCR_TOOLBAR_PREVIEW=1 时把工具条渲染成 PNG，供人工核对外观。
void OverlayToolbarTest::savePreviewWhenRequested()
{
    if (qEnvironmentVariableIsEmpty("SNAPCR_TOOLBAR_PREVIEW")) {
        QSKIP("preview disabled");
    }
    const QString outDir = QCoreApplication::applicationDirPath();

    OverlayToolbar selectionMode;
    selectionMode.setEditingMode(false);
    selectionMode.show();
    QVERIFY(QTest::qWaitForWindowExposed(&selectionMode));
    QVERIFY(selectionMode.grab().save(outDir + QStringLiteral("/toolbar_selection.png")));

    OverlayToolbar editingMode;
    editingMode.setEditingMode(true);
    editingMode.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editingMode));
    QVERIFY(editingMode.grab().save(outDir + QStringLiteral("/toolbar_editing.png")));

    // 提示气泡外观
    ToolbarTooltip* tooltip = ToolbarTooltip::instance();
    tooltip->showFor(&editingMode, QStringLiteral("复制到剪贴板 Enter"));
    QTest::qWait(50);  // offscreen 平台不支持 raise/expose，直接渲染即可
    QVERIFY(tooltip->width() > 40);
    QVERIFY(tooltip->grab().save(outDir + QStringLiteral("/toolbar_tooltip.png")));
    tooltip->hideNow();
}

QTEST_MAIN(OverlayToolbarTest)

#include "test_overlay_toolbar.moc"
