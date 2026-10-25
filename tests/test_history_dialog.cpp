#include <QtTest>

#include <QDir>
#include <QImage>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>

#include "history/HistoryStore.h"
#include "ui/HistoryDialog.h"

using namespace snapocr;

// 截图历史窗口（F-CAP-17）：列表/缩略图/删除联动，且能离屏渲染。
class HistoryDialogTest final : public QObject
{
    Q_OBJECT

private slots:
    void listsStoredEntries();
    void deleteRemovesEntry();
    void emptyStateShowsHint();
    void rendersThumbnails();

private:
    static void fillStore(history::HistoryStore* store, int count);
};

void HistoryDialogTest::fillStore(history::HistoryStore* store, int count)
{
    for (int i = 0; i < count; ++i) {
        QImage image(120, 80, QImage::Format_ARGB32);
        image.fill(QColor(20 + i * 30, 60 + i * 20, 200 - i * 20));
        store->add(image);
    }
}

void HistoryDialogTest::listsStoredEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    history::HistoryStore store;
    store.setDirectory(dir.path());
    fillStore(&store, 5);

    ui::HistoryDialog dialog(&store);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    auto* list = dialog.findChild<QListWidget*>(QStringLiteral("historyList"));
    QVERIFY(list != nullptr);
    QCOMPARE(list->count(), 5);
    QVERIFY(list->currentItem() != nullptr);  // 默认选中最新一张
}

void HistoryDialogTest::deleteRemovesEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    history::HistoryStore store;
    store.setDirectory(dir.path());
    fillStore(&store, 3);

    ui::HistoryDialog dialog(&store);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    auto* list = dialog.findChild<QListWidget*>(QStringLiteral("historyList"));
    QVERIFY(list != nullptr);
    QCOMPARE(list->count(), 3);

    // 找到"删除"按钮并点击
    QPushButton* deleteButton = nullptr;
    for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
        if (button->text().contains(QStringLiteral("删除"))) {
            deleteButton = button;
        }
    }
    QVERIFY(deleteButton != nullptr);
    deleteButton->click();
    QTest::qWait(20);

    QCOMPARE(store.entries().size(), 2);
    QCOMPARE(list->count(), 2);
}

void HistoryDialogTest::emptyStateShowsHint()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    history::HistoryStore store;
    store.setDirectory(dir.path());

    ui::HistoryDialog dialog(&store);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto* list = dialog.findChild<QListWidget*>(QStringLiteral("historyList"));
    QVERIFY(list != nullptr);
    QCOMPARE(list->count(), 0);
    QVERIFY(!list->isVisible());  // 空状态隐藏列表，显示提示文案
}

void HistoryDialogTest::rendersThumbnails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    history::HistoryStore store;
    store.setDirectory(dir.path());
    fillStore(&store, 6);

    ui::HistoryDialog dialog(&store);
    dialog.resize(760, 520);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QTest::qWait(50);

    const QImage rendered = dialog.grab().toImage();
    QVERIFY(!rendered.isNull());
    QVERIFY(rendered.width() > 400);

    // 缩略图区域必须有彩色像素（不是全白/全透明）
    int colored = 0;
    for (int y = 60; y < rendered.height() - 60; y += 5) {
        for (int x = 10; x < rendered.width() - 10; x += 5) {
            const QColor c = rendered.pixelColor(x, y);
            if (qAbs(c.red() - c.blue()) > 30) {
                ++colored;
            }
        }
    }
    QVERIFY2(colored > 30, qPrintable(QStringLiteral("colored=%1").arg(colored)));

    // 需要文档图时保存一份
    const QString previewPath = qEnvironmentVariable("SNAPCR_HISTORY_PREVIEW");
    if (!previewPath.isEmpty()) {
        rendered.save(previewPath, "PNG");
    }
}

QTEST_MAIN(HistoryDialogTest)

#include "test_history_dialog.moc"
