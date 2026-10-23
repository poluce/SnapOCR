#pragma once

#include <QString>

namespace snapocr::common {

// 安装日志处理器：写入 logDir 下按日期分文件，单文件 5MB，最多保留 3 个。
// level: debug / info / warning / error
void installLogging(const QString& logDir, const QString& level);

// 默认日志目录：%LOCALAPPDATA%/SnapOCR/logs
QString defaultLogDirectory();

}  // namespace snapocr::common
