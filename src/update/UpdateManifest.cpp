#include "update/UpdateManifest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

namespace snapocr::update {
namespace {

// 把 "1.2.3-rc.1" 拆成 主版本段 + 预发布段
void splitVersion(const QString& version, QVector<int>* numbers, QString* prerelease)
{
    numbers->clear();
    prerelease->clear();
    QString main = normalizeVersion(version);
    const int dash = main.indexOf('-');
    if (dash >= 0) {
        *prerelease = main.mid(dash + 1).trimmed();
        main = main.left(dash);
    }
    const QStringList parts = main.split('.', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        bool ok = false;
        const int value = part.toInt(&ok);
        numbers->append(ok ? value : 0);
    }
}

int compareNumberLists(const QVector<int>& a, const QVector<int>& b)
{
    const int count = qMax(a.size(), b.size());
    for (int index = 0; index < count; ++index) {
        const int left = index < a.size() ? a.at(index) : 0;
        const int right = index < b.size() ? b.at(index) : 0;
        if (left != right) {
            return left < right ? -1 : 1;
        }
    }
    return 0;
}

QString readLocalizedText(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

void fillFromAsset(const QJsonObject& asset, UpdateRelease* release)
{
    const QString name = asset.value(QStringLiteral("name")).toString();
    const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
    const qint64 size = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble());
    if (url.isEmpty()) {
        return;
    }
    if (name.endsWith(QStringLiteral("-setup.exe"), Qt::CaseInsensitive)) {
        if (!release->setupUrl.isValid()) {
            release->setupUrl = QUrl(url);
            release->setupName = name;
            release->setupSize = size;
        }
    } else if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
        if (!release->zipUrl.isValid()) {
            release->zipUrl = QUrl(url);
            release->zipSize = size;
        }
    }
}

}  // namespace

QString normalizeVersion(const QString& tag)
{
    QString version = tag.trimmed();
    if (version.startsWith(QLatin1Char('v')) || version.startsWith(QLatin1Char('V'))) {
        version.remove(0, 1);
    }
    return version.trimmed();
}

int compareVersions(const QString& a, const QString& b)
{
    QVector<int> aNumbers;
    QVector<int> bNumbers;
    QString aPre;
    QString bPre;
    splitVersion(a, &aNumbers, &aPre);
    splitVersion(b, &bNumbers, &bPre);

    const int numbers = compareNumberLists(aNumbers, bNumbers);
    if (numbers != 0) {
        return numbers;
    }
    // 同号：有预发布标识的更低（1.0.0-rc.1 < 1.0.0）
    const bool aHasPre = !aPre.isEmpty();
    const bool bHasPre = !bPre.isEmpty();
    if (!aHasPre && !bHasPre) {
        return 0;
    }
    if (aHasPre && !bHasPre) {
        return -1;
    }
    if (!aHasPre && bHasPre) {
        return 1;
    }
    // 预发布段里的数字按数值比较（rc.10 > rc.2），非数字按字典序
    const auto parts = [](const QString& text) {
        return text.split('.', Qt::SkipEmptyParts);
    };
    const QStringList aParts = parts(aPre);
    const QStringList bParts = parts(bPre);
    const int count = qMax(aParts.size(), bParts.size());
    for (int index = 0; index < count; ++index) {
        const QString left = index < aParts.size() ? aParts.at(index) : QString();
        const QString right = index < bParts.size() ? bParts.at(index) : QString();
        if (left == right) {
            continue;
        }
        bool leftOk = false;
        bool rightOk = false;
        const int leftNumber = left.toInt(&leftOk);
        const int rightNumber = right.toInt(&rightOk);
        if (leftOk && rightOk) {
            return leftNumber < rightNumber ? -1 : 1;
        }
        return left.compare(right, Qt::CaseInsensitive) < 0 ? -1 : 1;
    }
    return 0;
}

bool parseUpdatePayload(const QByteArray& payload, UpdateRelease* release,
                        QString* errorMessage)
{
    if (release == nullptr) {
        return false;
    }
    *release = UpdateRelease();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("更新源返回的不是有效 JSON：%1")
                                .arg(parseError.errorString());
        }
        return false;
    }
    const QJsonObject root = document.object();

    // GitHub Releases API
    if (root.contains(QStringLiteral("tag_name"))) {
        release->tagName = root.value(QStringLiteral("tag_name")).toString();
        release->version = normalizeVersion(release->tagName);
        release->notes = root.value(QStringLiteral("body")).toString();
        release->htmlUrl = root.value(QStringLiteral("html_url")).toString();
        const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& value : assets) {
            fillFromAsset(value.toObject(), release);
        }
        if (release->version.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("更新源缺少 tag_name");
            }
            return false;
        }
        return true;
    }

    // 自定义 manifest
    release->version = normalizeVersion(root.value(QStringLiteral("version")).toString());
    release->tagName = readLocalizedText(root, QStringLiteral("tagName"));
    if (release->tagName.isEmpty()) {
        release->tagName = QStringLiteral("v") + release->version;
    }
    release->notes = root.value(QStringLiteral("notes")).toString();
    release->htmlUrl = root.value(QStringLiteral("htmlUrl")).toString();
    const QString setupUrl = root.value(QStringLiteral("setupUrl")).toString();
    if (!setupUrl.isEmpty()) {
        release->setupUrl = QUrl(setupUrl);
        release->setupName = root.value(QStringLiteral("setupName")).toString();
        if (release->setupName.isEmpty()) {
            release->setupName =
                release->setupUrl.fileName(QUrl::FullyDecoded);
        }
        release->setupSize =
            static_cast<qint64>(root.value(QStringLiteral("setupSize")).toDouble());
    }
    const QString zipUrl = root.value(QStringLiteral("zipUrl")).toString();
    if (!zipUrl.isEmpty()) {
        release->zipUrl = QUrl(zipUrl);
        release->zipSize = static_cast<qint64>(root.value(QStringLiteral("zipSize")).toDouble());
    }
    if (release->version.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("manifest 缺少 version");
        }
        return false;
    }
    return true;
}

}  // namespace snapocr::update
