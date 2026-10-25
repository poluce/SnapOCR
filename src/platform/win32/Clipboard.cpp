#include "platform/win32/Clipboard.h"

#include <QBuffer>
#include <QByteArray>
#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMimeData>
#include <QThread>

namespace snapocr::platform::win32 {
namespace {

// 把 QImage 编码为指定格式（PNG/BMP 均由 Qt 内置插件支持）。
bool encodeImage(const QImage& image, const char* format, QByteArray* bytes)
{
    if (bytes == nullptr) {
        return false;
    }
    bytes->clear();
    QBuffer buffer(bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return false;
    }
    const bool saved = image.save(&buffer, format);
    buffer.close();
    return saved;
}

}  // namespace

bool copyImageToClipboard(const QImage& image, QString* errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    if (image.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("图像为空，无法复制到剪贴板");
        }
        return false;
    }

    // 剪贴板依赖 GUI 应用上下文；纯控制台/单元测试进程里直接给出原因而不是崩溃。
    if (qobject_cast<const QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("剪贴板不可用：当前进程没有 GUI 应用实例");
        }
        return false;
    }
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("剪贴板不可用：无法获取系统剪贴板");
        }
        return false;
    }

    QByteArray pngBytes;
    if (!encodeImage(image, "PNG", &pngBytes)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("图像编码为 PNG 失败，无法复制到剪贴板");
        }
        return false;
    }
    QByteArray bmpBytes;
    if (!encodeImage(image, "BMP", &bmpBytes)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("图像编码为 BMP 失败，无法复制到剪贴板");
        }
        return false;
    }

    // 同时提供 PNG、BMP(DIB) 与原始 QImage 数据：
    // - image/png 兼容 QQ/微信/浏览器等现代应用；
    // - image/bmp 是 Windows 剪贴板常用的 DIB 形态；
    // - setImageData 让 Qt 平台层再补一份 CF_DIB/CF_BITMAP。
    auto* mimeData = new QMimeData;
    mimeData->setData(QStringLiteral("image/png"), pngBytes);
    mimeData->setData(QStringLiteral("image/bmp"), bmpBytes);
    mimeData->setImageData(image);

    // setMimeData 接管所有权；Windows 上若剪贴板被其他进程短暂占用，
    // 失败由调用方提示用户重试（重试逻辑放在 UI 层，避免阻塞主线程）。
    clipboard->setMimeData(mimeData);
    return true;
}


bool copyTextToClipboard(const QString& text, QString* errorMessage)
{
    if (text.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("要复制的文本为空");
        }
        return false;
    }
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法访问剪贴板");
        }
        return false;
    }
    // 与本文件其余路径保持一致：走 Qt 的剪贴板封装（内部仍是 Win32 实现）。
    // 其他程序短暂占用剪贴板时 Qt 会静默失败（OleSetClipboard: OpenClipboard 失败），
    // 这里读回校验并重试，避免用户按 C 却什么都没复制。
    for (int attempt = 0; attempt < 5; ++attempt) {
        clipboard->setText(text, QClipboard::Clipboard);
        if (clipboard->text(QClipboard::Clipboard) == text) {
            return true;
        }
        QThread::msleep(40);
    }
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("写入剪贴板失败（可能被其他程序占用），请重试");
    }
    return false;
}

}  // namespace snapocr::platform::win32
