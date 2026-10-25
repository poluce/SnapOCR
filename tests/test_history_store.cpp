#include <QtTest>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include "history/HistoryStore.h"

using namespace snapocr::history;

// 截图历史（F-CAP-17）：落盘、去重、容量上限、删除、读回。
class HistoryStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void addSavesPngAndListsEntry();
    void consecutiveIdenticalImagesAreDeduped();
    void prunesToCapacity();
    void removeDeletesFile();
    void loadReturnsOriginalPixels();
    void emptyImageIsRejected();

private:
    static QImage makeImage(int width, int height, const QColor& color);
};

QImage HistoryStoreTest::makeImage(int width, int height, const QColor& color)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(color);
    return image;
}

void HistoryStoreTest::addSavesPngAndListsEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store;
    store.setDirectory(dir.path());

    const QImage image = makeImage(80, 60, QColor(0x1F, 0x6F, 0xEB));
    HistoryEntry entry;
    QVERIFY(store.add(image, &entry));
    QVERIFY(QFileInfo::exists(entry.filePath));
    QCOMPARE(entry.size, QSize(80, 60));

    const QVector<HistoryEntry> entries = store.entries();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().id, entry.id);
}

void HistoryStoreTest::consecutiveIdenticalImagesAreDeduped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store;
    store.setDirectory(dir.path());

    const QImage image = makeImage(40, 40, QColor(0xFF, 0x3B, 0x30));
    QVERIFY(store.add(image));
    // 复制后又贴图：同一张图连续入库只保留一条
    QVERIFY(!store.add(image));
    QCOMPARE(store.entries().size(), 1);

    // 不同内容正常入库
    QVERIFY(store.add(makeImage(40, 40, QColor(0x00, 0x00, 0x00))));
    QCOMPARE(store.entries().size(), 2);
}

void HistoryStoreTest::prunesToCapacity()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store;
    store.setDirectory(dir.path());

    const int total = HistoryStore::capacity() + 5;
    for (int i = 0; i < total; ++i) {
        QImage image(8, 8, QImage::Format_ARGB32);
        image.fill(QColor(i, 0, 0));  // 每张都不同
        QVERIFY(store.add(image));
    }
    QCOMPARE(store.entries().size(), HistoryStore::capacity());
    // 目录里也只保留 50 个文件
    const QStringList files = QDir(dir.path()).entryList({QStringLiteral("*.png")}, QDir::Files);
    QCOMPARE(files.size(), HistoryStore::capacity());
}

void HistoryStoreTest::removeDeletesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store;
    store.setDirectory(dir.path());

    HistoryEntry entry;
    QVERIFY(store.add(makeImage(30, 20, QColor(0x12, 0x34, 0x56)), &entry));
    QVERIFY(QFileInfo::exists(entry.filePath));

    QVERIFY(store.remove(entry.id));
    QVERIFY(!QFileInfo::exists(entry.filePath));
    QVERIFY(store.entries().isEmpty());
    QVERIFY(!store.remove(QStringLiteral("does-not-exist")));
}

void HistoryStoreTest::loadReturnsOriginalPixels()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store;
    store.setDirectory(dir.path());

    const QImage image = makeImage(64, 48, QColor(0x22, 0x8B, 0x22));
    HistoryEntry entry;
    QVERIFY(store.add(image, &entry));

    const QImage loaded = store.load(entry);
    QCOMPARE(loaded.size(), image.size());
    QCOMPARE(loaded.pixelColor(16, 12), image.pixelColor(16, 12));
}

void HistoryStoreTest::emptyImageIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store;
    store.setDirectory(dir.path());
    QVERIFY(!store.add(QImage()));
    QVERIFY(store.entries().isEmpty());
}

QTEST_GUILESS_MAIN(HistoryStoreTest)

#include "test_history_store.moc"
