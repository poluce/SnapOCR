#include "platform/win32/Console.h"

#include <cstdio>

#include <windows.h>

namespace snapocr::platform::win32 {
namespace {

// 判断进程是否已经继承到可用的标准输出句柄（控制台或重定向文件）。
bool hasInheritedStdout()
{
    const HANDLE handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    return ::GetFileType(handle) != FILE_TYPE_UNKNOWN;
}

}  // namespace

void attachParentConsole()
{
    if (hasInheritedStdout()) {
        return;
    }
    if (::AttachConsole(ATTACH_PARENT_PROCESS) == 0) {
        return;
    }

    std::FILE* stream = nullptr;
    (void)freopen_s(&stream, "CONOUT$", "w", stdout);
    (void)freopen_s(&stream, "CONOUT$", "w", stderr);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
}

}  // namespace snapocr::platform::win32
