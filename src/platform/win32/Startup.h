#pragma once

#include <QString>

namespace snapocr::platform::win32 {

// 注册/取消当前用户的登录自启（HKCU\Software\Microsoft\Windows\CurrentVersion\Run）。
bool setAutoStartEnabled(bool enabled, QString* errorMessage);
bool isAutoStartEnabled();

}  // namespace snapocr::platform::win32
