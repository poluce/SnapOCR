#include <QtTest>

#include <QToolButton>
#include <QWidget>

#include "overlay/OverlayToolbar.h"

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
    OverlayToolbar toolbar;
    toolbar.setEditingMode(true);
    toolbar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&toolbar));
    QCOMPARE(visibleRowCount(toolbar), 1);
    QVERIFY(toolbar.height() < 60);
    // 编辑模式比选择模式宽（多出撤销/颜色/线宽）。
    QVERIFY(toolbar.width() > 300);
}

void OverlayToolbarTest::hasAllActionButtons()
{
    OverlayToolbar toolbar;
    const auto buttons = toolbar.findChildren<QToolButton*>();
    // 1 选择 + 9 工具 + 2 撤销重做 + 2 颜色线宽 + 4 动作 = 18
    QCOMPARE(buttons.size(), 18);
    QVERIFY(!toolbar.findChild<QToolButton*>(QStringLiteral("copyButton"))->icon().isNull());
    QVERIFY(!toolbar.findChild<QToolButton*>(QStringLiteral("cancelButton"))->icon().isNull());
}

void OverlayToolbarTest::copyUsesCheckAndCancelUsesX()
{
    OverlayToolbar toolbar;
    auto* copy = toolbar.findChild<QToolButton*>(QStringLiteral("copyButton"));
    auto* cancel = toolbar.findChild<QToolButton*>(QStringLiteral("cancelButton"));
    QVERIFY(copy != nullptr);
    QVERIFY(cancel != nullptr);

    const QImage checkImage = copy->icon().pixmap(20, 20).toImage();
    const QImage crossImage = cancel->icon().pixmap(20, 20).toImage();
    QVERIFY(!checkImage.isNull());
    QVERIFY(!crossImage.isNull());
    // 勾号与叉号是不同图形。
    QVERIFY(checkImage != crossImage);

    // 叉号：右上与左下象限都有像素（对角线交点特征），勾号只有左下/右上趋势。
    auto inkCount = [](const QImage& image, int x0, int y0, int x1, int y1) {
        int count = 0;
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                if (qAlpha(image.pixel(x, y)) > 40) {
                    ++count;
                }
            }
        }
        return count;
    };
    const int w = crossImage.width();
    const int h = crossImage.height();
    // 叉号四个象限都应有笔画；勾号左上象限应当基本为空。
    const int crossTopLeft = inkCount(crossImage, 0, 0, w / 2, h / 2);
    const int crossTopRight = inkCount(crossImage, w / 2, 0, w, h / 2);
    QVERIFY(crossTopLeft > 0);
    QVERIFY(crossTopRight > 0);
    const int checkTopLeft = inkCount(checkImage, 0, 0, w / 2, h / 2);
    QVERIFY(checkTopLeft < crossTopLeft);
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
}

QTEST_MAIN(OverlayToolbarTest)

#include "test_overlay_toolbar.moc"
