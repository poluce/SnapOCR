#pragma once

#include <QString>

namespace snapocr::ui {

// 应用主题：light / dark / system（跟随系统）。
// 使用调色板 + 少量 QSS，保证覆盖层（深色半透明）不受影响。
void applyTheme(const QString& mode);

}  // namespace snapocr::ui
