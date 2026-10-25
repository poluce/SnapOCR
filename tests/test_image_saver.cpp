#include <QtTest>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

#include "app/ImageSaver.h"
#include "common/Settings.h"

using namespace snapocr;

// 保存流程（F-ANN-13/14）：按设置目录 + 模板 + 格式落盘，同名自动避免覆盖。
class ImageSaverTest final : public QObject
{
    Q_OBJECT

private slots:
    void savesWithTemplateAndFormat();
    void savesJpgWhenConfigured();
    void avoidsOverwritingExistingFile();

private:
    static QImage makeImage();
};

QImage ImageSaverTest::makeImage()
{
    QImage image(120, 80, QImage::Format_ARGB32);
    image.fill(QColor(0xFF, 0x3B, 0x30));
    return image;
}

void ImageSaverTest::savesWithTemplateAndFormat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    common::AppSettings settings;
    settings.saveDirectory = dir.path();
    settings.saveFormat = QStringLiteral("png");
    settings.nameTemplate = QStringLiteral("shot_yyyyMMdd");

    QString savedPath;
    QString error;
    QVERIFY2(app::ImageSaver::save(makeImage(), settings, &savedPath, &error),
             qPrintable(error));
    QVERIFY(QFileInfo::exists(savedPath));
    QVERIFY(savedPath.endsWith(QStringLiteral(".png")));
    QVERIFY(QFileInfo(savedPath).fileName().startsWith(QStringLiteral("shot_")));

    const QImage loaded(savedPath);
    QCOMPARE(loaded.size(), QSize(120, 80));
}

void ImageSaverTest::savesJpgWhenConfigured()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    common::AppSettings settings;
    settings.saveDirectory = dir.path();
    settings.saveFormat = QStringLiteral("jpg");
    settings.jpgQuality = 80;

    QString savedPath;
    QString error;
    QVERIFY2(app::ImageSaver::save(makeImage(), settings, &savedPath, &error),
             qPrintable(error));
    QVERIFY(savedPath.endsWith(QStringLiteral(".jpg")));
    QVERIFY(QFileInfo::exists(savedPath));
}

void ImageSaverTest::avoidsOverwritingExistingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    common::AppSettings settings;
    settings.saveDirectory = dir.path();
    settings.saveFormat = QStringLiteral("png");
    settings.nameTemplate = QStringLiteral("fixed_name");

    QString firstPath;
    QString secondPath;
    QString error;
    QVERIFY2(app::ImageSaver::save(makeImage(), settings, &firstPath, &error), qPrintable(error));
    QVERIFY2(app::ImageSaver::save(makeImage(), settings, &secondPath, &error), qPrintable(error));

    QVERIFY(QFileInfo::exists(firstPath));
    QVERIFY(QFileInfo::exists(secondPath));
    QVERIFY(firstPath != secondPath);  // 同名应自动加序号，不能覆盖
}

QTEST_MAIN(ImageSaverTest)

#include "test_image_saver.moc"
