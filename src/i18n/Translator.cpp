#include "i18n/Translator.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QTranslator>

namespace snapocr::i18n {
namespace {

QTranslator* g_translator = nullptr;
QString g_currentLanguage;

QStringList searchDirectories()
{
    QStringList directories;
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        directories << QDir(appDir).filePath(QStringLiteral("translations"));
    }
    directories << QStringLiteral(":/i18n");
    return directories;
}

// 把 locale 拆成候选文件名（精确 → 语言）。
QStringList candidateFileNames(const QString& locale)
{
    const QLocale parsed(locale);
    const QString full = parsed.name();
    const QString language = full.section(QLatin1Char('_'), 0, 0);
    QStringList names;
    if (!full.isEmpty()) {
        names << QStringLiteral("snapocr_%1.qm").arg(full);
    }
    if (!language.isEmpty() && language != full) {
        names << QStringLiteral("snapocr_%1.qm").arg(language);
    }
    return names;
}

}  // namespace

bool installUiTranslator(QApplication* app, const QString& locale)
{
    if (app == nullptr) {
        return false;
    }
    const QString normalized = locale.trimmed();
    // 中文是源语言：不需要翻译器
    if (normalized.isEmpty() || normalized.startsWith(QLatin1String("zh"), Qt::CaseInsensitive)) {
        if (g_translator != nullptr) {
            app->removeTranslator(g_translator);
            delete g_translator;
            g_translator = nullptr;
        }
        g_currentLanguage.clear();
        return false;
    }

    for (const QString& directory : searchDirectories()) {
        for (const QString& fileName : candidateFileNames(normalized)) {
            const bool inResource = directory.startsWith(QLatin1String(":/"));
            const QString path = inResource ? directory + QLatin1Char('/') + fileName
                                            : QDir(directory).filePath(fileName);
            if (!inResource && !QFileInfo::exists(path)) {
                continue;
            }
            auto* translator = new QTranslator;
            if (!translator->load(path)) {
                delete translator;
                continue;
            }
            if (g_translator != nullptr) {
                app->removeTranslator(g_translator);
                delete g_translator;
            }
            g_translator = translator;
            app->installTranslator(translator);
            g_currentLanguage = QLocale(normalized).name();
            qInfo("UI translator installed: %s (%s)", qPrintable(g_currentLanguage),
                  qPrintable(path));
            return true;
        }
    }
    return false;
}

QString currentUiLanguage()
{
    return g_currentLanguage;
}

QStringList availableUiLanguages()
{
    QStringList languages;
    for (const QString& directory : searchDirectories()) {
        QDir dir(directory);
        const QStringList files = dir.entryList({QStringLiteral("snapocr_*.qm")}, QDir::Files);
        for (const QString& file : files) {
            QString language = file;
            language.remove(QStringLiteral("snapocr_"));
            language.remove(QStringLiteral(".qm"));
            if (!languages.contains(language)) {
                languages << language;
            }
        }
    }
    return languages;
}

}  // namespace snapocr::i18n
