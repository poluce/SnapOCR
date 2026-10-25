#pragma once

#include <QColor>
#include <QDateTime>
#include <QKeySequence>
#include <QString>

namespace snapocr::common {

// 应用设置（持久化到 ini）。字段默认值即首次运行行为，新增字段必须在 load/save 中成对处理。
struct AppSettings
{
    // 常规
    QString language = QStringLiteral("zh-CN");
    bool startWithWindows = false;
    QString theme = QStringLiteral("system");  // light / dark / system

    // 热键（字符串形式，如 "F1"、"Ctrl+F1"）
    QString captureHotkey = QStringLiteral("F1");
    QString ocrHotkey = QStringLiteral("F4");

    // 捕获
    bool includeCursor = false;
    int delaySeconds = 0;

    // 编辑器
    QColor lastColor = QColor(QStringLiteral("#FF3B30"));
    int lastWidth = 3;
    int lastFontSize = 20;

    // OCR
    QString ocrEngine = QStringLiteral("windows");  // windows / rapidocr(阶段二)
    bool ocrAutoCopy = true;
    // 贴图：复制后是否自动贴到屏幕上（F-ANN-20）
    bool pinOnCopy = false;
    QString ocrLanguage;  // 空 = 跟随系统

    // 保存
    QString saveDirectory;  // 空 = 图片/SnapOCR
    QString saveFormat = QStringLiteral("png");  // png / jpg
    int jpgQuality = 90;
    QString nameTemplate = QStringLiteral("SnapOCR_yyyyMMdd_HHmmss");

    // 高级
    QString logLevel = QStringLiteral("info");
};

// 设置的读写与派生工具。纯逻辑，不依赖 UI，便于单测。
class SettingsStore
{
public:
    // filePath 为空时使用默认位置（便携模式：exe 同目录 SnapOCR.ini）。
    explicit SettingsStore(const QString& filePath = QString());

    AppSettings load(QString* warning = nullptr) const;
    bool save(const AppSettings& settings, QString* error = nullptr) const;
    void reset();

    QString filePath() const;
    bool exists() const;

    // 默认路径：便携模式为 exe 同目录，否则 %APPDATA%/SnapOCR/SnapOCR.ini
    static QString defaultFilePath();
    static bool portableModeRequested();

    // 默认保存目录（图片/SnapOCR，按系统“图片”已知文件夹解析）
    static QString defaultSaveDirectory();

    // 按模板生成文件名（不含目录与扩展名）。支持 yyyy/MM/dd/HH/mm/ss。
    static QString buildFileName(const QString& templateText, const QDateTime& now);

    // 解析扩展名对应的格式名（png/jpg），非法返回 png。
    static QString normalizedImageFormat(const QString& format);

private:
    QString m_filePath;
};

}  // namespace snapocr::common
