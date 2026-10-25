#pragma once

#include <QString>

namespace snapocr::platform::win32 {

// 崩溃记录（阶段三 §8.2）：未捕获异常/terminate 时把原因写进日志目录，
// 便于用户反馈问题时附上。实现只用 Win32 API（异常过滤器里不能依赖 Qt 内存分配）。
void installCrashReporter(const QString& logDirectory);

// 把异常码翻译成人话（纯函数，可单测）。
QString exceptionCodeText(quint32 code);

// 最近一次崩溃日志路径（没有则为空）。
QString lastCrashLogPath();

}  // namespace snapocr::platform::win32
