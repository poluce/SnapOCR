#include <QtTest>

#include "update/UpdateManifest.h"

using namespace snapocr::update;

// 更新源解析与版本比较（F-SYS-16）：纯逻辑，覆盖 GitHub API / 自定义 manifest 两种格式。
class UpdateManifestTest final : public QObject
{
    Q_OBJECT

private slots:
    void normalizeVersionStripsPrefix();
    void compareVersionsNumeric();
    void compareVersionsPrerelease();
    void parseGitHubRelease();
    void parseGitHubReleasePicksInstaller();
    void parseCustomManifest();
    void rejectInvalidPayload();
    void rejectManifestWithoutVersion();
};

void UpdateManifestTest::normalizeVersionStripsPrefix()
{
    QCOMPARE(normalizeVersion(QStringLiteral("v0.3.0")), QStringLiteral("0.3.0"));
    QCOMPARE(normalizeVersion(QStringLiteral("V1.2.3")), QStringLiteral("1.2.3"));
    QCOMPARE(normalizeVersion(QStringLiteral("  0.2.0 ")), QStringLiteral("0.2.0"));
}

void UpdateManifestTest::compareVersionsNumeric()
{
    QCOMPARE(compareVersions(QStringLiteral("0.2.0"), QStringLiteral("0.2.0")), 0);
    QCOMPARE(compareVersions(QStringLiteral("0.2.0"), QStringLiteral("0.3.0")), -1);
    QCOMPARE(compareVersions(QStringLiteral("0.10.0"), QStringLiteral("0.9.9")), 1);
    QCOMPARE(compareVersions(QStringLiteral("1.0"), QStringLiteral("1.0.0")), 0);
    QCOMPARE(compareVersions(QStringLiteral("v0.3.0"), QStringLiteral("0.2.9")), 1);
}

void UpdateManifestTest::compareVersionsPrerelease()
{
    QCOMPARE(compareVersions(QStringLiteral("1.0.0-rc.1"), QStringLiteral("1.0.0")), -1);
    QCOMPARE(compareVersions(QStringLiteral("1.0.0"), QStringLiteral("1.0.0-rc.1")), 1);
    QCOMPARE(compareVersions(QStringLiteral("1.0.0-rc.2"), QStringLiteral("1.0.0-rc.1")), 1);
    QCOMPARE(compareVersions(QStringLiteral("1.0.0-rc.10"), QStringLiteral("1.0.0-rc.2")), 1);
    QCOMPARE(compareVersions(QStringLiteral("1.0.0-beta"), QStringLiteral("1.0.0-beta")), 0);
}

void UpdateManifestTest::parseGitHubRelease()
{
    const QByteArray payload = R"({
      "tag_name": "v0.3.0",
      "name": "SnapOCR 0.3.0",
      "body": "修复若干问题\n新增在线更新",
      "html_url": "https://github.com/poluce/SnapOCR/releases/tag/v0.3.0",
      "assets": [
        {"name": "SnapOCR-0.3.0-win64.zip", "size": 1000,
         "browser_download_url": "https://example.com/SnapOCR-0.3.0-win64.zip"}
      ]
    })";
    UpdateRelease release;
    QString error;
    QVERIFY2(parseUpdatePayload(payload, &release, &error), qPrintable(error));
    QCOMPARE(release.version, QStringLiteral("0.3.0"));
    QCOMPARE(release.tagName, QStringLiteral("v0.3.0"));
    QVERIFY(release.notes.contains(QStringLiteral("在线更新")));
    QVERIFY(release.htmlUrl.contains(QStringLiteral("v0.3.0")));
    QVERIFY(!release.hasInstaller());
    QCOMPARE(release.zipUrl.toString(), QStringLiteral("https://example.com/SnapOCR-0.3.0-win64.zip"));
}

void UpdateManifestTest::parseGitHubReleasePicksInstaller()
{
    const QByteArray payload = R"({
      "tag_name": "v0.3.0",
      "body": "",
      "assets": [
        {"name": "SnapOCR-0.3.0-win64.zip", "size": 1000,
         "browser_download_url": "https://example.com/x.zip"},
        {"name": "SnapOCR-0.3.0-setup.exe", "size": 2000,
         "browser_download_url": "https://example.com/x-setup.exe"}
      ]
    })";
    UpdateRelease release;
    QVERIFY(parseUpdatePayload(payload, &release, nullptr));
    QVERIFY(release.hasInstaller());
    QCOMPARE(release.setupName, QStringLiteral("SnapOCR-0.3.0-setup.exe"));
    QCOMPARE(release.setupSize, qint64(2000));
    QVERIFY(release.zipUrl.isValid());
}

void UpdateManifestTest::parseCustomManifest()
{
    const QByteArray payload = R"({
      "version": "0.4.0",
      "notes": "自定义 manifest",
      "htmlUrl": "https://example.com/releases",
      "setupUrl": "https://example.com/SnapOCR-0.4.0-setup.exe",
      "setupSize": 3000,
      "zipUrl": "https://example.com/SnapOCR-0.4.0-win64.zip"
    })";
    UpdateRelease release;
    QString error;
    QVERIFY2(parseUpdatePayload(payload, &release, &error), qPrintable(error));
    QCOMPARE(release.version, QStringLiteral("0.4.0"));
    QCOMPARE(release.tagName, QStringLiteral("v0.4.0"));
    QVERIFY(release.hasInstaller());
    QCOMPARE(release.setupSize, qint64(3000));
    QVERIFY(release.zipUrl.isValid());
}

void UpdateManifestTest::rejectInvalidPayload()
{
    UpdateRelease release;
    QString error;
    QVERIFY(!parseUpdatePayload("<html>404</html>", &release, &error));
    QVERIFY(!error.isEmpty());
}

void UpdateManifestTest::rejectManifestWithoutVersion()
{
    UpdateRelease release;
    QString error;
    QVERIFY(!parseUpdatePayload(R"({"setupUrl":"https://example.com/a.exe"})", &release, &error));
    QVERIFY(error.contains(QStringLiteral("version")));
}

QTEST_GUILESS_MAIN(UpdateManifestTest)

#include "test_update_manifest.moc"
