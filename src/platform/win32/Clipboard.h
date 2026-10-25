#pragma once

#include <QImage>
#include <QString>

namespace snapocr::platform::win32 {

// 把图像写入系统剪贴板。同时提供 PNG 与 DIB，兼容 QQ/微信/Office。
// 返回 false 时 errorMessage 给出中文原因。
bool copyImageToClipboard(const QImage& image, QString* errorMessage);

// 把纯文本写入剪贴板（取色复制 HEX / RGB 用）。
bool copyTextToClipboard(const QString& text, QString* errorMessage);

}  // namespace snapocr::platform::win32
