#pragma once

#include <QLoggingCategory>

namespace snapocr::ocr {

// OCR 子系统日志分类（snapocr.ocr）。
// 约束：日志中禁止输出 OCR 原文、截图路径等用户敏感内容，只记录错误码/耗时/尺寸等元信息。
Q_DECLARE_LOGGING_CATEGORY(lcOcr)

}  // namespace snapocr::ocr
