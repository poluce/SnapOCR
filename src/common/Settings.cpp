#include "common/Settings.h"

#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeySequence>
#include <QSettings>
#include <QStandardPaths>
#include <QStringConverter>
#include <QStringList>
#include <QVariant>

namespace snapocr::common {
namespace {

// 与 ini schema 对应的合法取值范围；越界值一律回落默认值并告知调用方。
constexpr int kMinJpgQuality = 1;
constexpr int kMaxJpgQuality = 100;
constexpr int kMaxDelaySeconds = 300;
constexpr int kMinLineWidth = 1;
constexpr int kMaxLineWidth = 100;
constexpr int kMinFontSize = 8;
constexpr int kMaxFontSize = 200;

const QString kDefaultNameTemplate = QStringLiteral("SnapOCR_yyyyMMdd_HHmmss");
const QString kIllegalFileNameCharacters = QStringLiteral("\\/:*?\"<>|");
const QString kIniSuffix = QStringLiteral(".ini");

QString statusText(QSettings::Status status)
{
    switch (status) {
    case QSettings::NoError:
        return QStringLiteral("无错误");
    case QSettings::AccessError:
        return QStringLiteral("无法访问配置文件（权限不足或文件被占用）");
    case QSettings::FormatError:
        return QStringLiteral("配置文件格式错误");
    }
    return QStringLiteral("未知错误");
}

// UTF-8 解码；非法字节视为文件损坏（QSettings 对二进制垃圾过于宽容）。
bool decodeUtf8(const QByteArray& bytes, QString* text)
{
    if (text == nullptr) {
        return false;
    }
    static const QByteArray kUtf8Bom = QByteArrayLiteral("\xEF\xBB\xBF");
    QByteArray content = bytes;
    if (content.startsWith(kUtf8Bom)) {
        content.remove(0, kUtf8Bom.size());
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    *text = decoder.decode(content);
    return !decoder.hasError();
}

// 结构检查：合法 ini 至少要有节头 [x] 或 key=value，且不能有无法识别的内容行。
bool looksLikeIniDocument(const QString& text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    bool hasEntry = false;
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';'))
            || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            hasEntry = true;
            continue;
        }
        if (line.indexOf(QLatin1Char('=')) > 0) {
            hasEntry = true;
            continue;
        }
        // 既不是注释/节头，也不是 key=value 的行 → 不是有效 ini。
        return false;
    }
    return hasEntry;
}

// 损坏文件备份为 <path>.bak：优先重命名（避免随后保存覆盖唯一副本）。
void backupCorruptFile(const QString& filePath)
{
    const QString backupPath = filePath + QStringLiteral(".bak");
    (void)QFile::remove(backupPath);
    if (!QFile::rename(filePath, backupPath)) {
        (void)QFile::copy(filePath, backupPath);
    }
}

QString readTextValue(const QSettings& settings,
                      const QString& key,
                      const QString& fallback,
                      bool allowEmpty,
                      QStringList* issues)
{
    if (!settings.contains(key)) {
        return fallback;
    }
    const QString value = settings.value(key).toString().trimmed();
    if (value.isEmpty() && !allowEmpty) {
        issues->append(QStringLiteral("配置项 %1 为空，已使用默认值“%2”").arg(key, fallback));
        return fallback;
    }
    return value;
}

bool readBoolValue(const QSettings& settings,
                   const QString& key,
                   bool fallback,
                   QStringList* issues)
{
    if (!settings.contains(key)) {
        return fallback;
    }
    const QString value = settings.value(key).toString().trimmed().toLower();
    if (value == QStringLiteral("true") || value == QStringLiteral("1")) {
        return true;
    }
    if (value == QStringLiteral("false") || value == QStringLiteral("0")) {
        return false;
    }
    issues->append(QStringLiteral("配置项 %1 的值“%2”不是有效布尔值，已使用默认值")
                       .arg(key, value));
    return fallback;
}

int readIntValue(const QSettings& settings,
                 const QString& key,
                 int fallback,
                 int minimum,
                 int maximum,
                 QStringList* issues)
{
    if (!settings.contains(key)) {
        return fallback;
    }
    const QString text = settings.value(key).toString().trimmed();
    bool ok = false;
    const int value = text.toInt(&ok);
    if (!ok || value < minimum || value > maximum) {
        issues->append(
            QStringLiteral("配置项 %1 的值“%2”无效（应在 %3–%4 之间），已使用默认值 %5")
                .arg(key,
                     text,
                     QString::number(minimum),
                     QString::number(maximum),
                     QString::number(fallback)));
        return fallback;
    }
    return value;
}

QColor readColorValue(const QSettings& settings,
                      const QString& key,
                      const QColor& fallback,
                      QStringList* issues)
{
    if (!settings.contains(key)) {
        return fallback;
    }
    const QString text = settings.value(key).toString().trimmed();
    const QColor color = QColor::fromString(text);
    if (!color.isValid()) {
        issues->append(QStringLiteral("配置项 %1 的颜色值“%2”无效，已使用默认值 %3")
                           .arg(key, text, fallback.name(QColor::HexRgb).toUpper()));
        return fallback;
    }
    return color;
}

QString readHotkeyValue(const QSettings& settings,
                        const QString& key,
                        const QString& fallback,
                        QStringList* issues)
{
    if (!settings.contains(key)) {
        return fallback;
    }
    const QString text = settings.value(key).toString().trimmed();
    const QKeySequence sequence = QKeySequence::fromString(text, QKeySequence::PortableText);
    if (text.isEmpty() || sequence.isEmpty()) {
        issues->append(QStringLiteral("配置项 %1 的热键“%2”无效，已使用默认值 %3")
                           .arg(key, text, fallback));
        return fallback;
    }
    return text;
}

QString readEnumValue(const QSettings& settings,
                      const QString& key,
                      const QStringList& allowedValues,
                      const QString& fallback,
                      QStringList* issues)
{
    if (!settings.contains(key)) {
        return fallback;
    }
    const QString value = settings.value(key).toString().trimmed().toLower();
    if (!allowedValues.contains(value)) {
        issues->append(QStringLiteral("配置项 %1 的值“%2”不在允许范围（%3）内，已使用默认值“%4”")
                           .arg(key, value, allowedValues.join(QStringLiteral("/")), fallback));
        return fallback;
    }
    return value;
}

QString colorToHex(const QColor& color)
{
    if (!color.isValid()) {
        return QStringLiteral("#000000");
    }
    return color.name(QColor::HexRgb).toUpper();
}

QString fallbackConfigDirectory()
{
    return QDir::homePath() + QStringLiteral("/.snapocr");
}

}  // namespace

SettingsStore::SettingsStore(const QString& filePath)
    : m_filePath(filePath.trimmed().isEmpty() ? defaultFilePath()
                                              : QFileInfo(filePath).absoluteFilePath())
{
}

AppSettings SettingsStore::load(QString* warning) const
{
    if (warning != nullptr) {
        warning->clear();
    }

    AppSettings settings;  // 默认值即所有异常路径的兜底
    QStringList issues;

    const QFileInfo fileInfo(m_filePath);
    if (!fileInfo.exists()) {
        return settings;
    }
    if (!fileInfo.isFile()) {
        issues.append(QStringLiteral("配置路径 %1 不是普通文件，已使用默认设置").arg(m_filePath));
        if (warning != nullptr) {
            *warning = issues.join(QStringLiteral("；"));
        }
        return settings;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        issues.append(QStringLiteral("无法读取配置文件 %1：%2，已使用默认设置")
                          .arg(m_filePath, file.errorString()));
        if (warning != nullptr) {
            *warning = issues.join(QStringLiteral("；"));
        }
        return settings;
    }
    const QByteArray raw = file.readAll();
    file.close();

    if (raw.isEmpty()) {
        // 空文件等价于“尚未配置”，直接使用默认值，不视为损坏。
        return settings;
    }

    QString text;
    bool corrupt = false;
    if (!decodeUtf8(raw, &text)) {
        issues.append(QStringLiteral("配置文件包含非法 UTF-8 内容，可能已损坏"));
        corrupt = true;
    } else if (!looksLikeIniDocument(text)) {
        issues.append(QStringLiteral("配置文件不是有效的 INI 格式，可能已损坏"));
        corrupt = true;
    }
    if (corrupt) {
        backupCorruptFile(m_filePath);
        issues.append(QStringLiteral("原文件已备份为 %1.bak，本次使用默认设置").arg(m_filePath));
        if (warning != nullptr) {
            *warning = issues.join(QStringLiteral("；"));
        }
        return settings;
    }

    const QSettings ini(m_filePath, QSettings::IniFormat);
    // QSettings 在首次访问时才真正解析文件，这里主动触发加载并检查状态。
    ini.allKeys();
    if (ini.status() != QSettings::NoError) {
        issues.append(QStringLiteral("配置文件解析失败（%1）").arg(statusText(ini.status())));
        backupCorruptFile(m_filePath);
        issues.append(QStringLiteral("原文件已备份为 %1.bak，本次使用默认设置").arg(m_filePath));
        if (warning != nullptr) {
            *warning = issues.join(QStringLiteral("；"));
        }
        return settings;
    }

    // general
    settings.language =
        readTextValue(ini, QStringLiteral("general/language"), settings.language, false, &issues);
    settings.startWithWindows = readBoolValue(
        ini, QStringLiteral("general/startWithWindows"), settings.startWithWindows, &issues);
    settings.theme =
        readEnumValue(ini,
                      QStringLiteral("general/theme"),
                      {QStringLiteral("light"), QStringLiteral("dark"), QStringLiteral("system")},
                      settings.theme,
                      &issues);

    // hotkeys
    settings.captureHotkey = readHotkeyValue(
        ini, QStringLiteral("hotkeys/capture"), settings.captureHotkey, &issues);
    settings.ocrHotkey =
        readHotkeyValue(ini, QStringLiteral("hotkeys/ocr"), settings.ocrHotkey, &issues);

    // capture
    settings.includeCursor = readBoolValue(
        ini, QStringLiteral("capture/includeCursor"), settings.includeCursor, &issues);
    settings.delaySeconds = readIntValue(ini,
                                         QStringLiteral("capture/delaySeconds"),
                                         settings.delaySeconds,
                                         0,
                                         kMaxDelaySeconds,
                                         &issues);

    // editor
    settings.lastColor =
        readColorValue(ini, QStringLiteral("editor/lastColor"), settings.lastColor, &issues);
    settings.lastWidth = readIntValue(ini,
                                      QStringLiteral("editor/lastWidth"),
                                      settings.lastWidth,
                                      kMinLineWidth,
                                      kMaxLineWidth,
                                      &issues);
    settings.lastFontSize = readIntValue(ini,
                                         QStringLiteral("editor/lastFontSize"),
                                         settings.lastFontSize,
                                         kMinFontSize,
                                         kMaxFontSize,
                                         &issues);

    // ocr
    settings.ocrEngine =
        readEnumValue(ini,
                      QStringLiteral("ocr/engine"),
                      {QStringLiteral("windows"), QStringLiteral("rapidocr")},
                      settings.ocrEngine,
                      &issues);
    settings.ocrAutoCopy =
        readBoolValue(ini, QStringLiteral("ocr/autoCopy"), settings.ocrAutoCopy, &issues);
    settings.pinOnCopy =
        readBoolValue(ini, QStringLiteral("editor/pinOnCopy"), settings.pinOnCopy, &issues);
    settings.ocrLanguage =
        readTextValue(ini, QStringLiteral("ocr/language"), settings.ocrLanguage, true, &issues);

    // save
    settings.saveDirectory = readTextValue(
        ini, QStringLiteral("save/directory"), settings.saveDirectory, true, &issues);
    settings.saveFormat = readEnumValue(ini,
                                        QStringLiteral("save/format"),
                                        {QStringLiteral("png"),
                                         QStringLiteral("jpg"),
                                         QStringLiteral("jpeg")},
                                        settings.saveFormat,
                                        &issues);
    settings.saveFormat = normalizedImageFormat(settings.saveFormat);
    settings.jpgQuality = readIntValue(ini,
                                       QStringLiteral("save/jpgQuality"),
                                       settings.jpgQuality,
                                       kMinJpgQuality,
                                       kMaxJpgQuality,
                                       &issues);
    settings.nameTemplate = readTextValue(
        ini, QStringLiteral("save/nameTemplate"), settings.nameTemplate, false, &issues);

    // advanced
    settings.logLevel =
        readEnumValue(ini,
                      QStringLiteral("advanced/logLevel"),
                      {QStringLiteral("debug"),
                       QStringLiteral("info"),
                       QStringLiteral("warning"),
                       QStringLiteral("error")},
                      settings.logLevel,
                      &issues);

    if (warning != nullptr && !issues.isEmpty()) {
        *warning = issues.join(QStringLiteral("；"));
    }
    return settings;
}

bool SettingsStore::save(const AppSettings& settings, QString* error) const
{
    if (error != nullptr) {
        error->clear();
    }

    const QString directory = QFileInfo(m_filePath).absolutePath();
    if (directory.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("配置文件的目录路径无效：%1").arg(m_filePath);
        }
        return false;
    }
    if (!QDir().mkpath(directory)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法创建配置目录：%1").arg(directory);
        }
        return false;
    }

    QSettings ini(m_filePath, QSettings::IniFormat);
    // 全量写入：先清空，避免旧版本遗留键与新 schema 混用。
    ini.clear();

    ini.setValue(QStringLiteral("general/language"), settings.language);
    ini.setValue(QStringLiteral("general/startWithWindows"), settings.startWithWindows);
    ini.setValue(QStringLiteral("general/theme"), settings.theme);

    ini.setValue(QStringLiteral("hotkeys/capture"), settings.captureHotkey);
    ini.setValue(QStringLiteral("hotkeys/ocr"), settings.ocrHotkey);

    ini.setValue(QStringLiteral("capture/includeCursor"), settings.includeCursor);
    ini.setValue(QStringLiteral("capture/delaySeconds"), settings.delaySeconds);

    ini.setValue(QStringLiteral("editor/lastColor"), colorToHex(settings.lastColor));
    ini.setValue(QStringLiteral("editor/lastWidth"), settings.lastWidth);
    ini.setValue(QStringLiteral("editor/lastFontSize"), settings.lastFontSize);

    ini.setValue(QStringLiteral("ocr/engine"), settings.ocrEngine);
    ini.setValue(QStringLiteral("ocr/autoCopy"), settings.ocrAutoCopy);
    ini.setValue(QStringLiteral("editor/pinOnCopy"), settings.pinOnCopy);
    ini.setValue(QStringLiteral("ocr/language"), settings.ocrLanguage);

    ini.setValue(QStringLiteral("save/directory"), settings.saveDirectory);
    ini.setValue(QStringLiteral("save/format"), normalizedImageFormat(settings.saveFormat));
    ini.setValue(QStringLiteral("save/jpgQuality"), settings.jpgQuality);
    ini.setValue(QStringLiteral("save/nameTemplate"), settings.nameTemplate);

    ini.setValue(QStringLiteral("advanced/logLevel"), settings.logLevel);

    ini.sync();
    if (ini.status() != QSettings::NoError) {
        if (error != nullptr) {
            *error = QStringLiteral("写入配置文件失败（%1）：%2")
                         .arg(statusText(ini.status()), m_filePath);
        }
        return false;
    }
    // sync() 成功但目标被目录等占用时，仍以文件真实存在为准。
    if (!QFileInfo(m_filePath).isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("配置文件未生成：%1").arg(m_filePath);
        }
        return false;
    }
    return true;
}

void SettingsStore::reset()
{
    (void)QFile::remove(m_filePath);
}

QString SettingsStore::filePath() const
{
    return m_filePath;
}

bool SettingsStore::exists() const
{
    return QFileInfo(m_filePath).isFile();
}

QString SettingsStore::defaultFilePath()
{
    if (portableModeRequested()) {
        return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("SnapOCR.ini"));
    }

    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (directory.isEmpty()) {
        directory = fallbackConfigDirectory();
    }
    return QDir(directory).filePath(QStringLiteral("SnapOCR") + kIniSuffix);
}

bool SettingsStore::portableModeRequested()
{
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    if (applicationDirectory.isEmpty()) {
        return false;
    }
    return QFileInfo::exists(
        QDir(applicationDirectory).filePath(QStringLiteral("SnapOCR.portable")));
}

QString SettingsStore::defaultSaveDirectory()
{
    QString picturesDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesDirectory.isEmpty()) {
        picturesDirectory = QDir::homePath() + QStringLiteral("/Pictures");
    }
    return QDir(picturesDirectory).filePath(QStringLiteral("SnapOCR"));
}

QString SettingsStore::buildFileName(const QString& templateText, const QDateTime& now)
{
    QString pattern = templateText;
    if (pattern.trimmed().isEmpty()) {
        pattern = kDefaultNameTemplate;
    }

    // 逐 token 替换：模板里可能混有 y/M/d/H/m/s 之外的普通字符，不能整串交给
    // QDateTime::toString（否则会被当成格式串解释）。
    static const QStringList kDateTokens = {
        QStringLiteral("yyyy"),
        QStringLiteral("MM"),
        QStringLiteral("dd"),
        QStringLiteral("HH"),
        QStringLiteral("mm"),
        QStringLiteral("ss"),
    };

    QString fileName;
    fileName.reserve(pattern.size() + 16);
    qsizetype index = 0;
    while (index < pattern.size()) {
        bool replaced = false;
        for (const QString& token : kDateTokens) {
            if (pattern.mid(index, token.size()) == token) {
                fileName += now.toString(token);
                index += token.size();
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            fileName += pattern.at(index);
            ++index;
        }
    }

    // Windows 文件名非法字符与不可见控制字符统一替换为下划线。
    for (qsizetype characterIndex = 0; characterIndex < fileName.size(); ++characterIndex) {
        const QChar character = fileName.at(characterIndex);
        if (kIllegalFileNameCharacters.contains(character) || character.unicode() < 0x20) {
            fileName[characterIndex] = QLatin1Char('_');
        }
    }
    return fileName;
}

QString SettingsStore::normalizedImageFormat(const QString& format)
{
    const QString normalized = format.trimmed().toLower();
    if (normalized == QStringLiteral("jpg") || normalized == QStringLiteral("jpeg")) {
        return QStringLiteral("jpg");
    }
    return QStringLiteral("png");
}

}  // namespace snapocr::common
