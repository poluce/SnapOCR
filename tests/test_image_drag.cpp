#include <QtTest>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

#include "ui/ImageDrag.h"

using namespace snapocr::ui;

// 拖拽导出（F-SYS-12）：mime 里必须同时有文件 URL 与 PNG 位图数据，
// 这样资源管理器（按文件）和聊天气泡（按位图）都能接收。
class ImageDragTest final : public QObject
{
    Q_OBJECT

private slots:
    void imageMimeDataWritesTempPng();
    void fileMimeDataUsesExistingFile();
    void nullImageProducesNoMimeData();
    void missingFileProducesNoMimeData();
};

void ImageDragTest::imageMimeDataWritesTempPng()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage image(64, 48, QImage::Format_ARGB32);
    image.fill(QColor(0x1F, 0x6F, 0xEB));

    QString tempPath;
    QMimeData* data = imageMimeData(image, dir.path(), &tempPath);
    QVERIFY(data != nullptr);
    QVERIFY(!tempPath.isEmpty());
    QVERIFY(QFileInfo::exists(tempPath));
    QVERIFY(tempPath.endsWith(QStringLiteral(".png")));

    // 文件 URL
    QVERIFY(data->hasUrls());
    QCOMPARE(data->urls().first().toLocalFile(), tempPath);
    // PNG 位图数据
    QVERIFY(data->hasFormat(QStringLiteral("image/png")));
    // 文本路径（部分输入框只认文本）
    QVERIFY(data->hasText());
    QCOMPARE(data->text(), tempPath);

    const QImage fromFile(tempPath);
    QCOMPARE(fromFile.size(), image.size());

    delete data;
}

void ImageDragTest::fileMimeDataUsesExistingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("shot.png"));
    QImage image(20, 10, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(path, "PNG"));

    QMimeData* data = fileMimeData(path);
    QVERIFY(data != nullptr);
    QVERIFY(data->hasUrls());
    QCOMPARE(data->urls().first().toLocalFile(), path);
    QVERIFY(data->hasFormat(QStringLiteral("image/png")));
    delete data;
}

void ImageDragTest::nullImageProducesNoMimeData()
{
    QVERIFY(imageMimeData(QImage(), QString(), nullptr) == nullptr);
}

void ImageDragTest::missingFileProducesNoMimeData()
{
    QVERIFY(fileMimeData(QStringLiteral("C:/definitely/not/here.png")) == nullptr);
    QVERIFY(fileMimeData(QString()) == nullptr);
}

QTEST_GUILESS_MAIN(ImageDragTest)

#include "test_image_drag.moc"
