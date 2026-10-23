#include <QtTest>

#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

#include "common/Settings.h"

using snapocr::common::AppSettings;
using snapocr::common::SettingsStore;

namespace {

void verifyDefaultSettings(const AppSettings& settings)
{
    QCOMPARE(settings.language, QStringLiteral("zh-CN"));
    QCOMPARE(settings.startWithWindows, false);
    QCOMPARE(settings.theme, QStringLiteral("system"));
    QCOMPARE(settings.captureHotkey, QStringLiteral("F1"));
    QCOMPARE(settings.ocrHotkey, QStringLiteral("F4"));
    QCOMPARE(settings.includeCursor, false);
    QCOMPARE(settings.delaySeconds, 0);
    QCOMPARE(settings.lastColor.name(QColor::HexRgb).toUpper(), QStringLiteral("#FF3B30"));
    QCOMPARE(settings.lastWidth, 3);
    QCOMPARE(settings.lastFontSize, 20);
    QCOMPARE(settings.ocrEngine, QStringLiteral("windows"));
    QCOMPARE(settings.ocrAutoCopy, true);
    QCOMPARE(settings.ocrLanguage, QString());
    QCOMPARE(settings.saveDirectory, QString());
    QCOMPARE(settings.saveFormat, QStringLiteral("png"));
    QCOMPARE(settings.jpgQuality, 90);
    QCOMPARE(settings.nameTemplate, QStringLiteral("SnapOCR_yyyyMMdd_HHmmss"));
    QCOMPARE(settings.logLevel, QStringLiteral("info"));
}

bool writeRawFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content) == content.size();
}

}  // namespace

class SettingsTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadReturnsDefaultsWhenFileMissing();
    void saveThenLoadRoundTripsAllFields();
    void corruptedBinaryFileFallsBackToDefaults();
    void corruptedTextFileFallsBackToDefaults();
    void invalidValuesFallBackPerField();
    void resetDeletesFile();
    void saveFailsWithChineseErrorWhenPathIsDirectory();
    void buildFileNameReplacesDateTokens();
    void buildFileNameReplacesIllegalCharacters();
    void buildFileNameFallsBackForEmptyTemplate();
    void normalizedImageFormatHandlesAliases();
    void defaultPathsAreUsable();
    void emptyConstructorUsesDefaultPath();
};

void SettingsTest::loadReturnsDefaultsWhenFileMissing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const SettingsStore store(directory.filePath(QStringLiteral("SnapOCR.ini")));
    QVERIFY(!store.exists());

    QString warning = QStringLiteral("占位内容，load 应主动清空");
    const AppSettings settings = store.load(&warning);
    QVERIFY(warning.isEmpty());
    verifyDefaultSettings(settings);
}

void SettingsTest::saveThenLoadRoundTripsAllFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    // 嵌套目录同时验证 save() 会自动 mkpath。
    const QString path = directory.filePath(QStringLiteral("nested/config/SnapOCR.ini"));
    const SettingsStore store(path);

    AppSettings expected;
    expected.language = QStringLiteral("en-US");
    expected.startWithWindows = true;
    expected.theme = QStringLiteral("dark");
    expected.captureHotkey = QStringLiteral("Ctrl+F1");
    expected.ocrHotkey = QStringLiteral("Ctrl+Shift+F4");
    expected.includeCursor = true;
    expected.delaySeconds = 5;
    expected.lastColor = QColor(QStringLiteral("#00AAFF"));
    expected.lastWidth = 7;
    expected.lastFontSize = 32;
    expected.ocrEngine = QStringLiteral("rapidocr");
    expected.ocrAutoCopy = false;
    expected.ocrLanguage = QStringLiteral("zh-Hans-CN");
    expected.saveDirectory = QStringLiteral("D:/shots");
    expected.saveFormat = QStringLiteral("jpg");
    expected.jpgQuality = 75;
    expected.nameTemplate = QStringLiteral("shot-yyyyMMdd_HHmmss");
    expected.logLevel = QStringLiteral("debug");

    QString error;
    QVERIFY2(store.save(expected, &error), qPrintable(error));
    QVERIFY(store.exists());
    QVERIFY(QFileInfo::exists(path));

    QString warning;
    const AppSettings loaded = store.load(&warning);
    QVERIFY2(warning.isEmpty(), qPrintable(warning));

    QCOMPARE(loaded.language, expected.language);
    QCOMPARE(loaded.startWithWindows, expected.startWithWindows);
    QCOMPARE(loaded.theme, expected.theme);
    QCOMPARE(loaded.captureHotkey, expected.captureHotkey);
    QCOMPARE(loaded.ocrHotkey, expected.ocrHotkey);
    QCOMPARE(loaded.includeCursor, expected.includeCursor);
    QCOMPARE(loaded.delaySeconds, expected.delaySeconds);
    QCOMPARE(loaded.lastColor.name(QColor::HexRgb).toUpper(),
             expected.lastColor.name(QColor::HexRgb).toUpper());
    QCOMPARE(loaded.lastWidth, expected.lastWidth);
    QCOMPARE(loaded.lastFontSize, expected.lastFontSize);
    QCOMPARE(loaded.ocrEngine, expected.ocrEngine);
    QCOMPARE(loaded.ocrAutoCopy, expected.ocrAutoCopy);
    QCOMPARE(loaded.ocrLanguage, expected.ocrLanguage);
    QCOMPARE(loaded.saveDirectory, expected.saveDirectory);
    QCOMPARE(loaded.saveFormat, expected.saveFormat);
    QCOMPARE(loaded.jpgQuality, expected.jpgQuality);
    QCOMPARE(loaded.nameTemplate, expected.nameTemplate);
    QCOMPARE(loaded.logLevel, expected.logLevel);
}

void SettingsTest::corruptedBinaryFileFallsBackToDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("SnapOCR.ini"));

    QByteArray garbage = QStringLiteral("这不是合法的 ini 内容").toUtf8();
    garbage.append('\xFF');
    garbage.append('\xFE');
    garbage.append('\0');
    garbage.append("trailing");
    QVERIFY(writeRawFile(path, garbage));

    const SettingsStore store(path);
    QString warning;
    const AppSettings settings = store.load(&warning);

    QVERIFY(!warning.isEmpty());
    QVERIFY(warning.contains(QStringLiteral("损坏")));
    QVERIFY(warning.contains(QStringLiteral("备份")));
    verifyDefaultSettings(settings);
    // 损坏文件应被备份保留，避免随后保存覆盖唯一副本。
    QVERIFY(QFileInfo::exists(path + QStringLiteral(".bak")));
}

void SettingsTest::corruptedTextFileFallsBackToDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("SnapOCR.ini"));

    // 合法 UTF-8，但不是 ini（无节头、无 key=value），应判定为损坏。
    const QByteArray garbage =
        QStringLiteral("### 这是一段乱码\n没有等号也没有节头\n###").toUtf8();
    QVERIFY(writeRawFile(path, garbage));

    const SettingsStore store(path);
    QString warning;
    const AppSettings settings = store.load(&warning);
    QVERIFY(!warning.isEmpty());
    verifyDefaultSettings(settings);
    QVERIFY(QFileInfo::exists(path + QStringLiteral(".bak")));
}

void SettingsTest::invalidValuesFallBackPerField()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("SnapOCR.ini"));
    {
        QSettings ini(path, QSettings::IniFormat);
        ini.setValue(QStringLiteral("general/language"), QStringLiteral("en-US"));  // 合法，保留
        ini.setValue(QStringLiteral("general/startWithWindows"), QStringLiteral("maybe"));  // 非法
        ini.setValue(QStringLiteral("general/theme"), QStringLiteral("neon"));  // 非法
        ini.setValue(QStringLiteral("hotkeys/capture"), QString());  // 空
        ini.setValue(QStringLiteral("editor/lastColor"), QStringLiteral("not-a-color"));  // 非法
        ini.setValue(QStringLiteral("editor/lastWidth"), 999);  // 越界
        ini.setValue(QStringLiteral("editor/lastFontSize"), 28);  // 合法，保留
        ini.setValue(QStringLiteral("save/format"), QStringLiteral("bmp"));  // 不支持
        ini.setValue(QStringLiteral("save/jpgQuality"), 500);  // 越界
        ini.setValue(QStringLiteral("advanced/logLevel"), QStringLiteral("trace"));  // 非法
        ini.sync();
        QCOMPARE(ini.status(), QSettings::NoError);
    }

    const SettingsStore store(path);
    QString warning;
    const AppSettings settings = store.load(&warning);

    QVERIFY(!warning.isEmpty());
    QVERIFY(warning.contains(QStringLiteral("jpgQuality")));
    // 非法项回落默认值
    QCOMPARE(settings.startWithWindows, false);
    QCOMPARE(settings.theme, QStringLiteral("system"));
    QCOMPARE(settings.captureHotkey, QStringLiteral("F1"));
    QCOMPARE(settings.lastColor.name(QColor::HexRgb).toUpper(), QStringLiteral("#FF3B30"));
    QCOMPARE(settings.lastWidth, 3);
    QCOMPARE(settings.saveFormat, QStringLiteral("png"));
    QCOMPARE(settings.jpgQuality, 90);
    QCOMPARE(settings.logLevel, QStringLiteral("info"));
    // 合法项保持原值
    QCOMPARE(settings.language, QStringLiteral("en-US"));
    QCOMPARE(settings.lastFontSize, 28);
}

void SettingsTest::resetDeletesFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsStore store(directory.filePath(QStringLiteral("SnapOCR.ini")));

    QString error;
    QVERIFY2(store.save(AppSettings{}, &error), qPrintable(error));
    QVERIFY(store.exists());

    store.reset();
    QVERIFY(!store.exists());

    QString warning = QStringLiteral("占位");
    const AppSettings settings = store.load(&warning);
    QVERIFY(warning.isEmpty());
    verifyDefaultSettings(settings);
}

void SettingsTest::saveFailsWithChineseErrorWhenPathIsDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("SnapOCR.ini"));
    // 用同名目录占位，使 QSettings 无法把该路径当作配置文件写入。
    QVERIFY(QDir().mkpath(path));

    const SettingsStore store(path);
    QString error;
    QVERIFY(!store.save(AppSettings{}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains(QStringLiteral("配置")));
}

void SettingsTest::buildFileNameReplacesDateTokens()
{
    const QDateTime now(QDate(2026, 1, 2), QTime(3, 4, 5));
    QCOMPARE(SettingsStore::buildFileName(QStringLiteral("SnapOCR_yyyyMMdd_HHmmss"), now),
             QStringLiteral("SnapOCR_20260102_030405"));
    QCOMPARE(SettingsStore::buildFileName(QStringLiteral("IMG-yyyy-MM-dd_HH-mm-ss"), now),
             QStringLiteral("IMG-2026-01-02_03-04-05"));
    QCOMPARE(SettingsStore::buildFileName(QStringLiteral("截图_MMdd_HHmm"), now),
             QStringLiteral("截图_0102_0304"));
    // 非 token 的字符（包括 m/d 等单字母）原样保留。
    QCOMPARE(SettingsStore::buildFileName(QStringLiteral("m-d_H"), now),
             QStringLiteral("m-d_H"));
}

void SettingsTest::buildFileNameReplacesIllegalCharacters()
{
    const QDateTime now(QDate(2026, 10, 23), QTime(13, 45, 59));
    QCOMPARE(SettingsStore::buildFileName(QStringLiteral("a\\b/c:d*e?f\"g<h>i|j"), now),
             QStringLiteral("a_b_c_d_e_f_g_h_i_j"));
}

void SettingsTest::buildFileNameFallsBackForEmptyTemplate()
{
    const QDateTime now(QDate(2026, 10, 23), QTime(13, 45, 59));
    const QString expected = QStringLiteral("SnapOCR_20261023_134559");
    QCOMPARE(SettingsStore::buildFileName(QString(), now), expected);
    QCOMPARE(SettingsStore::buildFileName(QStringLiteral("   "), now), expected);
}

void SettingsTest::normalizedImageFormatHandlesAliases()
{
    QCOMPARE(SettingsStore::normalizedImageFormat(QStringLiteral("JPG")), QStringLiteral("jpg"));
    QCOMPARE(SettingsStore::normalizedImageFormat(QStringLiteral("jpeg")), QStringLiteral("jpg"));
    QCOMPARE(SettingsStore::normalizedImageFormat(QStringLiteral(" JPEG ")), QStringLiteral("jpg"));
    QCOMPARE(SettingsStore::normalizedImageFormat(QStringLiteral("png")), QStringLiteral("png"));
    QCOMPARE(SettingsStore::normalizedImageFormat(QStringLiteral("Png")), QStringLiteral("png"));
    QCOMPARE(SettingsStore::normalizedImageFormat(QStringLiteral("bmp")), QStringLiteral("png"));
    QCOMPARE(SettingsStore::normalizedImageFormat(QString()), QStringLiteral("png"));
}

void SettingsTest::defaultPathsAreUsable()
{
    const QString configPath = SettingsStore::defaultFilePath();
    QVERIFY(!configPath.isEmpty());
    QVERIFY(QDir::isAbsolutePath(configPath));
    QVERIFY(configPath.endsWith(QStringLiteral("SnapOCR.ini")));

    const QString saveDirectory = SettingsStore::defaultSaveDirectory();
    QVERIFY(!saveDirectory.isEmpty());
    QVERIFY(QDir::isAbsolutePath(saveDirectory));
    QVERIFY(saveDirectory.endsWith(QStringLiteral("SnapOCR")));

    // 便携模式检测必须与 exe 同目录标记文件的存在性一致（不创建标记，避免污染构建目录）。
    const QString markerPath =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("SnapOCR.portable"));
    QCOMPARE(SettingsStore::portableModeRequested(), QFileInfo::exists(markerPath));
}

void SettingsTest::emptyConstructorUsesDefaultPath()
{
    const SettingsStore store;
    QCOMPARE(store.filePath(), SettingsStore::defaultFilePath());
}

QTEST_GUILESS_MAIN(SettingsTest)

#include "test_settings.moc"
