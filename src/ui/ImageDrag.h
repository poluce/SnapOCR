#pragma once

#include <QImage>
#include <QString>

class QMimeData;
class QWidget;

namespace snapocr::ui {

// 图片拖拽导出（F-SYS-12）：把截图拖到资源管理器/微信/Word 等目标。
//
// 实现方式：写一个临时 PNG，再在 QMimeData 里给出 text/uri-list（文件路径）
// 与 image/png 两份数据——前者让资源管理器/IM 接收为文件，后者兼容只认位图的程序。

// 把图片写成临时文件并返回带 URL 的 mime 数据（调用方接管所有权）。
// tempDirectory 为空时使用系统临时目录；tempFilePath 返回写出的文件路径。
QMimeData* imageMimeData(const QImage& image, const QString& tempDirectory, QString* tempFilePath);

// 已有文件（如截图历史里的 PNG）直接构造 mime 数据，不重复拷贝。
QMimeData* fileMimeData(const QString& filePath);

// 立即开始一次拖拽（阻塞直到拖放结束）；image 为空时返回 false。
bool startImageDrag(QWidget* source, const QImage& image);

}  // namespace snapocr::ui
