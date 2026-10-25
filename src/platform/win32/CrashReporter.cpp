#include "platform/win32/CrashReporter.h"

#include <QDir>
#include <QFileInfo>

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <exception>

namespace snapocr::platform::win32 {
namespace {

wchar_t g_logDirectory[MAX_PATH] = {};
wchar_t g_lastCrashPath[MAX_PATH] = {};

// 异常过滤器里不能调用 Qt（可能已处于不稳定状态），这里只走 Win32。
void writeCrashFile(quint32 code, void* address, const char* reason)
{
    if (g_logDirectory[0] == L'\0') {
        return;
    }
    SYSTEMTIME localTime{};
    ::GetLocalTime(&localTime);

    wchar_t path[MAX_PATH] = {};
    ::swprintf(path, MAX_PATH, L"%s\\crash-%04d%02d%02d-%02d%02d%02d.txt", g_logDirectory,
               localTime.wYear, localTime.wMonth, localTime.wDay, localTime.wHour,
               localTime.wMinute, localTime.wSecond);

    char buffer[1024] = {};
    const int length = ::_snprintf(
        buffer, sizeof(buffer),
        "SnapOCR crash report\r\nreason: %s\r\nexception code: 0x%08lX\r\naddress: %p\r\ntime: "
        "%04d-%02d-%02d %02d:%02d:%02d\r\n",
        reason, static_cast<unsigned long>(code), address, localTime.wYear, localTime.wMonth,
        localTime.wDay, localTime.wHour, localTime.wMinute, localTime.wSecond);
    if (length <= 0) {
        return;
    }

    const HANDLE file = ::CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    ::WriteFile(file, buffer, static_cast<DWORD>(length), &written, nullptr);
    ::CloseHandle(file);
    ::wcsncpy(g_lastCrashPath, path, MAX_PATH - 1);
    g_lastCrashPath[MAX_PATH - 1] = L'\0';
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info)
{
    quint32 code = 0;
    void* address = nullptr;
    if (info != nullptr && info->ExceptionRecord != nullptr) {
        code = static_cast<quint32>(info->ExceptionRecord->ExceptionCode);
        address = reinterpret_cast<void*>(info->ExceptionRecord->ExceptionAddress);
    }
    writeCrashFile(code, address, "unhandled exception");
    return EXCEPTION_EXECUTE_HANDLER;
}

void terminateHandler()
{
    writeCrashFile(0, nullptr, "std::terminate");
    ::TerminateProcess(::GetCurrentProcess(), 3);
}

}  // namespace

QString exceptionCodeText(quint32 code)
{
    switch (code) {
    case 0xC0000005:
        return QStringLiteral("访问冲突（0xC0000005）");
    case 0xC00000FD:
        return QStringLiteral("栈溢出（0xC00000FD）");
    case 0xC0000374:
        return QStringLiteral("堆损坏（0xC0000374）");
    case 0xC0000409:
        return QStringLiteral("快速失败（0xC0000409）");
    case 0xE06D7363:
        return QStringLiteral("未捕获的 C++ 异常（0xE06D7363）");
    default:
        return QStringLiteral("异常 0x%1").arg(code, 8, 16, QLatin1Char('0')).toUpper();
    }
}

void installCrashReporter(const QString& logDirectory)
{
    if (logDirectory.isEmpty()) {
        return;
    }
    const QString nativePath = QDir::toNativeSeparators(logDirectory);
    if (nativePath.isEmpty()) {
        return;
    }
    ::wcsncpy(g_logDirectory, reinterpret_cast<const wchar_t*>(nativePath.utf16()),
              MAX_PATH - 1);
    g_logDirectory[MAX_PATH - 1] = L'\0';

    QDir().mkpath(logDirectory);
    ::SetUnhandledExceptionFilter(unhandledExceptionFilter);
    std::set_terminate(terminateHandler);
}

QString lastCrashLogPath()
{
    return QString::fromWCharArray(g_lastCrashPath);
}

}  // namespace snapocr::platform::win32
