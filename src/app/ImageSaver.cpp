#include "app/ImageSaver.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageWriter>
#include <QWidget>

namespace snapocr::app {
namespace {

QString resolvedDirectory(const common::AppSettings& settings)
{
    if (!settings.saveDirectory.isEmpty()) {
        return settings.saveDirectory;
    }
    return common::SettingsStore::defaultSaveDirectory();
}

QString resolvedExtension(const common::AppSettings& settings)
{
    return common::SettingsStore::normalizedImageFormat(settings.saveFormat)
               == QLatin1String("jpg")
               ? QStringLiteral("jpg")
               : QStringLiteral("png");
}

bool writeImage(const QImage& image, const QString& path, const common::AppSettings& settings,
                QString* errorMessage)
{
    QImageWriter writer(path);
    if (path.endsWith(QLatin1String(".jpg"), Qt::CaseInsensitive)) {
        writer.setFormat("jpg");
        writer.setQuality(qBound(1, settings.jpgQuality, 100));
    } else {
        writer.setFormat("png");
    }
    if (!writer.write(image)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("保存图片失败：%1").arg(writer.errorString());
        }
        return false;
    }
    return true;
}

}  // namespace

bool ImageSaver::save(const QImage& image, const common::AppSettings& settings,
                      QString* savedPath, QString* errorMessage)
{
    if (image.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("图像为空，无法保存");
        }
        return false;
    }

    const QString directory = resolvedDirectory(settings);
    if (!QDir().mkpath(directory)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法创建保存目录：%1").arg(directory);
        }
        return false;
    }

    const QString baseName =
        common::SettingsStore::buildFileName(settings.nameTemplate, QDateTime::currentDateTime());
    const QString extension = resolvedExtension(settings);
    QString path = QDir(directory).filePath(QStringLiteral("%1.%2").arg(baseName, extension));

    // 同名文件追加序号，避免覆盖。
    int suffix = 1;
    while (QFileInfo::exists(path) && suffix < 1000) {
        path = QDir(directory).filePath(
            QStringLiteral("%1_%2.%3").arg(baseName).arg(suffix).arg(extension));
        ++suffix;
    }

    if (!writeImage(image, path, settings, errorMessage)) {
        return false;
    }
    if (savedPath != nullptr) {
        *savedPath = path;
    }
    return true;
}

bool ImageSaver::saveAs(const QImage& image, const common::AppSettings& settings,
                        QWidget* parentDialog, QString* savedPath, QString* errorMessage)
{
    if (image.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("图像为空，无法保存");
        }
        return false;
    }

    const QString directory = resolvedDirectory(settings);
    QDir().mkpath(directory);
    const QString baseName =
        common::SettingsStore::buildFileName(settings.nameTemplate, QDateTime::currentDateTime());
    QString path = QDir(directory).filePath(QStringLiteral("%1.png").arg(baseName));

    path = QFileDialog::getSaveFileName(parentDialog, QStringLiteral("另存为"), path,
                                        QStringLiteral("PNG 图片 (*.png);;JPEG 图片 (*.jpg)"));
    if (path.isEmpty()) {
        return false;  // 用户取消
    }
    if (!path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)
        && !path.endsWith(QLatin1String(".jpg"), Qt::CaseInsensitive)
        && !path.endsWith(QLatin1String(".jpeg"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".png");
    }

    if (!writeImage(image, path, settings, errorMessage)) {
        return false;
    }
    if (savedPath != nullptr) {
        *savedPath = path;
    }
    return true;
}

}  // namespace snapocr::app
