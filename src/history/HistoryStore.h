#pragma once

#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVector>

namespace snapocr::history {

// 一条历史记录：磁盘上的 PNG + 元信息。
struct HistoryEntry
{
    QString id;  // 文件名（不含扩展名），也是删除用的键
    QDateTime capturedAt;
    QSize size;  // 物理像素尺寸
    QString filePath;
};

// 截图历史（F-CAP-17）：最近 50 张，落盘保存，重启后仍在。
class HistoryStore final : public QObject
{
    Q_OBJECT

public:
    explicit HistoryStore(QObject* parent = nullptr);

    static QString defaultHistoryDirectory();
    static int capacity() { return 50; }

    void setDirectory(const QString& directory);
    QString directory() const { return m_directory; }

    // 保存一张截图到历史；与最近一条内容完全相同时跳过（避免“复制+贴图”重复入库）。
    // 返回 true 表示新入库，false 表示跳过或失败。
    bool add(const QImage& image, HistoryEntry* stored = nullptr);

    // 新 → 旧。
    QVector<HistoryEntry> entries() const;

    bool remove(const QString& id);
    QImage load(const HistoryEntry& entry) const;

signals:
    void changed();

private:
    void pruneTo(int limit);
    static QVector<HistoryEntry> scan(const QString& directory);

    QString m_directory;
    QString m_lastHash;
};

}  // namespace snapocr::history
