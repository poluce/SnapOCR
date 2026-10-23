#pragma once

namespace snapocr::platform::win32 {

// WIN32 GUI 子系统进程默认没有控制台。
// 从 cmd/PowerShell 启动时尝试挂接父控制台，使 --version/--help/--smoke-test
// 的标准输出可见；从资源管理器启动时静默返回，不影响正常 GUI 运行。
void attachParentConsole();

}  // namespace snapocr::platform::win32
