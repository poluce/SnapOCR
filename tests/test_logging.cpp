#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "common/Logging.h"
#include "common/Settings.h"

using namespace snapocr::common;

// 便携模式（F-SYS-11）：exe 同目录存在 SnapOCR.portable 时，
// 配置与日志都必须落在 exe 旁边，方便整个文件夹拷走。
class LoggingTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void portableMarkerRedirectsLogDirectory();
    void withoutMarkerUsesUserDataDirectory();

private:
    QString m_markerPath;
    bool m_markerCreated = false;
};

void LoggingTest::init()
{
    m_markerPath =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("SnapOCR.portable"));
    QFile::remove(m_markerPath);  // 保证从非便携状态开始
    m_markerCreated = false;
}

void LoggingTest::cleanup()
{
    if (m_markerCreated) {
        QFile::remove(m_markerPath);
        m_markerCreated = false;
    }
}

void LoggingTest::portableMarkerRedirectsLogDirectory()
{
    QFile marker(m_markerPath);
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();
    m_markerCreated = true;

    QVERIFY(SettingsStore::portableModeRequested());

    const QString logDirectory = defaultLogDirectory();
    QVERIFY(!logDirectory.isEmpty());
    QVERIFY(QDir::isAbsolutePath(logDirectory));
    QCOMPARE(QDir::cleanPath(logDirectory),
             QDir::cleanPath(QDir(QCoreApplication::applicationDirPath())
                                 .filePath(QStringLiteral("logs"))));

    // 配置也要跟着 exe 走
    const QString configPath = SettingsStore::defaultFilePath();
    QCOMPARE(QDir::cleanPath(QFileInfo(configPath).absolutePath()),
             QDir::cleanPath(QCoreApplication::applicationDirPath()));
}

void LoggingTest::withoutMarkerUsesUserDataDirectory()
{
    QVERIFY(!SettingsStore::portableModeRequested());
    const QString logDirectory = defaultLogDirectory();
    QVERIFY(!logDirectory.isEmpty());
    QVERIFY(QDir::isAbsolutePath(logDirectory));
    QVERIFY(QDir::cleanPath(logDirectory) !=
            QDir::cleanPath(QDir(QCoreApplication::applicationDirPath())
                                .filePath(QStringLiteral("logs"))));
}

QTEST_GUILESS_MAIN(LoggingTest)

#include "test_logging.moc"
