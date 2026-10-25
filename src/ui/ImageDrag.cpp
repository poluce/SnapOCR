#include "ui/ImageDrag.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QDrag>
#include <QFileInfo>
#include <QMimeData>
#include <QPixmap>
#include <QWidget>
#include <QStandardPaths>
#include <QUrl>

namespace snapocr::ui {
namespace {

QString uniqueTempPath(const QString& directory)
{
    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    return QDir(directory).filePath(QStringLiteral("SnapOCR_%1.png").arg(stamp));
}

void attachImageBytes(QMimeData* data, const QImage& image)
{
    if (image.isNull() || data == nullptr) {
        return;
    }
    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    if (buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG")) {
        data->setData(QStringLiteral("image/png"), pngBytes);
    }
}

}  // namespace

QMimeData* imageMimeData(const QImage& image, const QString& tempDirectory, QString* tempFilePath)
{
    if (image.isNull()) {
        return nullptr;
    }
    QString directory = tempDirectory;
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (directory.isEmpty()) {
            directory = QDir::tempPath();
        }
    }
    QDir().mkpath(directory);

    const QString path = uniqueTempPath(directory);
    if (!image.save(path, "PNG")) {
        return nullptr;
    }
    if (tempFilePath != nullptr) {
        *tempFilePath = path;
    }

    auto* data = new QMimeData;
    data->setUrls({QUrl::fromLocalFile(path)});
    data->setText(path);  // 某些目标（输入框）只接受文本路径
    attachImageBytes(data, image);
    return data;
}

QMimeData* fileMimeData(const QString& filePath)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        return nullptr;
    }
    auto* data = new QMimeData;
    data->setUrls({QUrl::fromLocalFile(filePath)});
    data->setText(filePath);

    const QImage image(filePath);
    attachImageBytes(data, image);
    return data;
}

bool startImageDrag(QWidget* source, const QImage& image)
{
    if (source == nullptr || image.isNull()) {
        return false;
    }
    QMimeData* data = imageMimeData(image, QString(), nullptr);
    if (data == nullptr) {
        return false;
    }
    auto* drag = new QDrag(source);  // QDrag 自行管理生命周期
    drag->setMimeData(data);
    drag->setPixmap(QPixmap::fromImage(
        image.scaled(160, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    drag->exec(Qt::CopyAction, Qt::CopyAction);
    return true;
}

}  // namespace snapocr::ui
