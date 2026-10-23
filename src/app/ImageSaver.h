#pragma once

#include <QImage>
#include <QString>

#include "common/Settings.h"

namespace snapocr::app {

// 图像保存：按设置中的目录/格式/文件名模板落盘。
class ImageSaver
{
public:
    // 保存成功时 savedPath 返回完整路径；失败时 errorMessage 给出中文原因。
    static bool save(const QImage& image, const common::AppSettings& settings, QString* savedPath,
                     QString* errorMessage);

    // 另存为：弹出文件对话框（用户取消时返回 false 且不设置 errorMessage）。
    static bool saveAs(const QImage& image, const common::AppSettings& settings,
                       QWidget* parentDialog, QString* savedPath, QString* errorMessage);
};

}  // namespace snapocr::app
