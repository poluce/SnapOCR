#include "common/Logging.h"

#include <cstdio>

#include <QDate>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QStringList>
#include <QtLogging>

namespace snapocr::common {
namespace {

// 单文件上限 5MB，同一日期最多保留 3 个文件（当前文件 + .1 + .2）。
constexpr qint64 kMaxLogFileBytes = 5 * 1024 * 1024;
constexpr int kMaxLogFileCount = 3;

const QString kLogFilePrefix = QStringLiteral("snapocr-");
const QString kLogFileSuffix = QStringLiteral(".log");

// 日志级别过滤阈值（数值越小越详细）。
constexpr int kRankDebug = 0;
constexpr int kRankInfo = 1;
constexpr int kRankWarning = 2;
constexpr int kRankError = 3;

struct LogState
{
    QMutex mutex;
    QString directory;
    int threshold = kRankInfo;
    QtMessageHandler previousHandler = nullptr;
    QFile file;
    QDate fileDate;
    qint64 bytesWritten = 0;
    bool installed = false;
    bool fileUnavailable = false;
};

// 故意使用“永不析构”的静态对象：qInstallMessageHandler 安装的处理器可能在
// main 返回后、静态对象析构期间仍被调用，普通静态对象此时已不可用。
LogState& logState()
{
    static LogState* state = new LogState;
    return *state;
}

int parseLevel(const QString& level)
{
    const QString normalized = level.trimmed().toLower();
    if (normalized == QStringLiteral("debug")) {
        return kRankDebug;
    }
    if (normalized == QStringLiteral("warning") || normalized == QStringLiteral("warn")) {
        return kRankWarning;
    }
    if (normalized == QStringLiteral("error") || normalized == QStringLiteral("critical")) {
        return kRankError;
    }
    // 空值或未知级别按 info 处理；设置已在 SettingsStore 中校验，这里只做兜底。
    return kRankInfo;
}

int messageRank(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return kRankDebug;
    case QtInfoMsg:
        return kRankInfo;
    case QtWarningMsg:
        return kRankWarning;
    case QtCriticalMsg:
    case QtFatalMsg:
        return kRankError;
    }
    return kRankError;
}

QString messageLevelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARNING");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

QString buildLogLine(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    QString text = message;
    while (text.endsWith(QLatin1Char('\n'))) {
        text.chop(1);
    }
    QString category =
        context.category != nullptr ? QString::fromUtf8(context.category) : QString();
    if (category.isEmpty()) {
        category = QStringLiteral("default");
    }
    // 单次多参数 arg：避免消息正文中的 "%1" 等占位符被后续替换。
    return QStringLiteral("%1 [%2] %3: %4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
             messageLevelName(type),
             category,
             text);
}

bool ensureDirectory(const QString& directory)
{
    if (directory.isEmpty()) {
        return false;
    }
    QDir dir(directory);
    if (dir.exists()) {
        return true;
    }
    return QDir().mkpath(directory);
}

// 只保留指定日期的日志轮转链，避免跨天文件无限累积（与“最多保留 3 个”一致）。
void removeLogsOfOtherDays(const QString& directory, const QDate& keepDate)
{
    QDir dir(directory);
    const QStringList names =
        dir.entryList({kLogFilePrefix + QStringLiteral("*") + kLogFileSuffix + QStringLiteral("*")},
                      QDir::Files);
    const QString keepPrefix = kLogFilePrefix + keepDate.toString(QStringLiteral("yyyyMMdd"));
    for (const QString& name : names) {
        if (name.startsWith(keepPrefix)) {
            continue;
        }
        (void)QFile::remove(dir.filePath(name));
    }
}

bool openLogFileLocked(LogState* state, const QDate& date)
{
    if (state->file.isOpen()) {
        state->file.close();
    }
    removeLogsOfOtherDays(state->directory, date);

    const QString fileName =
        kLogFilePrefix + date.toString(QStringLiteral("yyyyMMdd")) + kLogFileSuffix;
    const QString path = QDir(state->directory).filePath(fileName);
    state->file.setFileName(path);
    if (!state->file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        state->fileUnavailable = true;
        return false;
    }
    state->fileDate = date;
    state->bytesWritten = state->file.size();
    return true;
}

// base.log → base.log.1 → base.log.2，最旧文件丢弃；保持总数不超过 kMaxLogFileCount。
bool rotateLogFileLocked(LogState* state)
{
    const QString basePath = state->file.fileName();
    if (state->file.isOpen()) {
        state->file.close();
    }

    (void)QFile::remove(basePath + QStringLiteral(".%1").arg(kMaxLogFileCount - 1));
    for (int index = kMaxLogFileCount - 2; index >= 1; --index) {
        const QString from = basePath + QStringLiteral(".%1").arg(index);
        const QString to = basePath + QStringLiteral(".%1").arg(index + 1);
        if (QFile::exists(from)) {
            (void)QFile::rename(from, to);
        }
    }
    if (QFile::exists(basePath)) {
        (void)QFile::rename(basePath, basePath + QStringLiteral(".1"));
    }

    state->file.setFileName(basePath);
    if (!state->file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        state->fileUnavailable = true;
        return false;
    }
    state->bytesWritten = 0;
    return true;
}

bool writeLineLocked(LogState* state, const QString& line)
{
    if (state->directory.isEmpty()) {
        return false;
    }

    const QDate today = QDate::currentDate();
    if (!state->file.isOpen() || state->fileDate != today) {
        if (!openLogFileLocked(state, today)) {
            return false;
        }
    }

    QByteArray utf8 = line.toUtf8();
    utf8.append('\n');
    if (state->bytesWritten > 0 && state->bytesWritten + utf8.size() > kMaxLogFileBytes) {
        if (!rotateLogFileLocked(state)) {
            return false;
        }
    }

    const qint64 written = state->file.write(utf8);
    if (written != static_cast<qint64>(utf8.size())) {
        return false;
    }
    state->file.flush();
    state->bytesWritten += written;
    return true;
}

void logMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    LogState& state = logState();
    QtMessageHandler previousHandler = nullptr;
    QString line;

    {
        QMutexLocker locker(&state.mutex);
        if (messageRank(type) < state.threshold) {
            return;
        }
        line = buildLogLine(type, context, message);
        if (state.installed && !state.fileUnavailable) {
            if (writeLineLocked(&state, line)) {
                return;
            }
            // 运行期写文件失败（磁盘满/权限变化等）：静默降级，避免每条日志重复尝试。
            state.fileUnavailable = true;
        }
        // 目录不可写等情况下静默降级：把消息交还给上一个处理器（默认输出到调试器/stderr）。
        previousHandler = state.previousHandler;
    }

    // 不持锁回调外部处理器，避免对方再次写日志时死锁。
    if (previousHandler != nullptr) {
        previousHandler(type, context, message);
        return;
    }
    const QByteArray utf8 = line.toUtf8();
    (void)std::fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
    (void)std::fputc('\n', stderr);
    (void)std::fflush(stderr);
}

}  // namespace

void installLogging(const QString& logDir, const QString& level)
{
    LogState& state = logState();
    QMutexLocker locker(&state.mutex);

    if (!state.installed) {
        state.previousHandler = qInstallMessageHandler(logMessageHandler);
        state.installed = true;
    }

    state.threshold = parseLevel(level);

    const QString directory = logDir.trimmed().isEmpty() ? defaultLogDirectory() : logDir;
    if (state.directory != directory) {
        if (state.file.isOpen()) {
            state.file.close();
        }
        state.fileDate = QDate();
        state.bytesWritten = 0;
    }
    state.directory = directory;
    // 目录预创建失败时不抛错，处理器在首次写文件失败后自动降级到 stderr/上一个处理器。
    state.fileUnavailable = !ensureDirectory(directory);
}

QString defaultLogDirectory()
{
    // 便携模式：exe 同目录存在 SnapOCR.portable 时，日志写到 exe 旁边的 logs/，
    // 与配置文件保持一致（配置路径见 SettingsStore::defaultFilePath）。
    const QCoreApplication* application = QCoreApplication::instance();
    if (application != nullptr && QFileInfo::exists(QDir(QCoreApplication::applicationDirPath())
                                                        .filePath(QStringLiteral(
                                                            "SnapOCR.portable")))) {
        return QDir(QDir(QCoreApplication::applicationDirPath())
                        .filePath(QStringLiteral("logs")))
            .absolutePath();
    }

    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/AppData/Local/SnapOCR");
    }
    return QDir(base).filePath(QStringLiteral("logs"));
}

}  // namespace snapocr::common
