#include "history/HistoryStore.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>

namespace snapocr::history {
namespace {

constexpr char kExtension[] = ".png";

QString hashImage(const QImage& image)
{
    // 只用于“和最近一张是否相同”的去重，不需要密码学强度。
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(QByteArray(reinterpret_cast<const char*>(image.constBits()),
                            static_cast<qsizetype>(image.sizeInBytes())));
    hash.addData(QByteArray::number(image.width()));
    hash.addData(QByteArray::number(image.height()));
    return QString::fromLatin1(hash.result().toHex());
}

}  // namespace

HistoryStore::HistoryStore(QObject* parent)
    : QObject(parent)
    , m_directory(defaultHistoryDirectory())
{
}

QString HistoryStore::defaultHistoryDirectory()
{
    // 便携模式跟随 exe，与配置/日志保持一致。
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    if (!applicationDirectory.isEmpty()
        && QFileInfo::exists(QDir(applicationDirectory).filePath(QStringLiteral("SnapOCR.portable")))) {
        return QDir(applicationDirectory).filePath(QStringLiteral("history"));
    }

    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::temp().filePath(QStringLiteral("SnapOCR"));
    }
    return QDir(base).filePath(QStringLiteral("history"));
}

void HistoryStore::setDirectory(const QString& directory)
{
    m_directory = directory;
}

QVector<HistoryEntry> HistoryStore::scan(const QString& directory)
{
    QVector<HistoryEntry> result;
    const QDir dir(directory);
    if (!dir.exists()) {
        return result;
    }
    const QFileInfoList files =
        dir.entryInfoList({QStringLiteral("*") + QString::fromLatin1(kExtension)}, QDir::Files,
                          QDir::Time);  // 新 → 旧
    result.reserve(files.size());
    for (const QFileInfo& info : files) {
        HistoryEntry entry;
        entry.id = info.completeBaseName();
        entry.capturedAt = info.lastModified();
        entry.filePath = info.absoluteFilePath();
        const QSize size = QImage(info.absoluteFilePath()).size();
        entry.size = size;
        result.append(entry);
    }
    return result;
}

QVector<HistoryEntry> HistoryStore::entries() const
{
    return scan(m_directory);
}

bool HistoryStore::add(const QImage& image, HistoryEntry* stored)
{
    if (image.isNull()) {
        return false;
    }
    const QString hash = hashImage(image);
    if (hash == m_lastHash) {
        return false;  // 同一张图连续入库（复制后又贴图）只留一条
    }

    if (!QDir().mkpath(m_directory)) {
        return false;
    }

    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QString baseName = stamp;
    QString path = QDir(m_directory).filePath(baseName + QString::fromLatin1(kExtension));
    int suffix = 1;
    while (QFileInfo::exists(path)) {
        baseName = QStringLiteral("%1_%2").arg(stamp).arg(suffix++);
        path = QDir(m_directory).filePath(baseName + QString::fromLatin1(kExtension));
    }

    if (!image.save(path, "PNG")) {
        return false;
    }

    m_lastHash = hash;
    pruneTo(capacity());

    if (stored != nullptr) {
        stored->id = baseName;
        stored->capturedAt = QDateTime::currentDateTime();
        stored->size = image.size();
        stored->filePath = path;
    }
    emit changed();
    return true;
}

bool HistoryStore::remove(const QString& id)
{
    if (id.isEmpty()) {
        return false;
    }
    const QString path = QDir(m_directory).filePath(id + QString::fromLatin1(kExtension));
    const bool removed = QFile::remove(path);
    if (removed) {
        emit changed();
    }
    return removed;
}

QImage HistoryStore::load(const HistoryEntry& entry) const
{
    return QImage(entry.filePath);
}

void HistoryStore::pruneTo(int limit)
{
    QVector<HistoryEntry> all = scan(m_directory);
    if (all.size() <= limit) {
        return;
    }
    // scan 按时间新→旧，超出部分即最旧的
    for (int i = limit; i < all.size(); ++i) {
        QFile::remove(all.at(i).filePath);
    }
}

}  // namespace snapocr::history
