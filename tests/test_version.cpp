#include <QtTest>

#include <QRegularExpression>
#include <QString>

#include "snapocr/Version.h"

class VersionTest final : public QObject
{
    Q_OBJECT

private slots:
    void appNameIsStable();
    void versionIsSemantic();
    void singleInstanceKeyIsStable();
};

void VersionTest::appNameIsStable()
{
    QCOMPARE(QString::fromUtf8(snapocr::version::kAppName), QStringLiteral("SnapOCR"));
}

void VersionTest::versionIsSemantic()
{
    const QString version = QString::fromUtf8(snapocr::version::kVersion);
    const QRegularExpression pattern(QStringLiteral(R"(^\d+\.\d+\.\d+$)"));
    QVERIFY2(pattern.match(version).hasMatch(), qPrintable(version));
}

void VersionTest::singleInstanceKeyIsStable()
{
    const QString key = QString::fromUtf8(snapocr::version::kSingleInstanceKey);
    QVERIFY(key.startsWith(QStringLiteral("SnapOCR.")));
}

QTEST_GUILESS_MAIN(VersionTest)

#include "test_version.moc"
