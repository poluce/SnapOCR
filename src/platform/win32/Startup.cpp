#include "platform/win32/Startup.h"

#include <string>

#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QString>

#include <windows.h>

namespace snapocr::platform::win32 {
namespace {

const wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t kAutoStartValueName[] = L"SnapOCR";

Q_LOGGING_CATEGORY(lcStartup, "snapocr.platform.startup")

// RAII 关闭注册表句柄，所有提前返回路径都不会泄漏。
class RegistryKey
{
public:
    RegistryKey() = default;
    ~RegistryKey()
    {
        if (m_key != nullptr) {
            ::RegCloseKey(m_key);
        }
    }

    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(const RegistryKey&) = delete;

    HKEY* address()
    {
        return &m_key;
    }

    HKEY get() const
    {
        return m_key;
    }

private:
    HKEY m_key = nullptr;
};

QString win32ErrorMessage(LONG code)
{
    switch (code) {
    case ERROR_ACCESS_DENIED:
        return QStringLiteral("没有权限访问注册表（错误码 %1）").arg(code);
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return QStringLiteral("注册表项不存在（错误码 %1）").arg(code);
    default:
        break;
    }

    wchar_t* buffer = nullptr;
    const DWORD length = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
                                              | FORMAT_MESSAGE_FROM_SYSTEM
                                              | FORMAT_MESSAGE_IGNORE_INSERTS,
                                          nullptr,
                                          static_cast<DWORD>(code),
                                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                          reinterpret_cast<LPWSTR>(&buffer),
                                          0,
                                          nullptr);
    QString message;
    if (length > 0 && buffer != nullptr) {
        message = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
    }
    if (buffer != nullptr) {
        ::LocalFree(buffer);
    }
    if (message.isEmpty()) {
        message = QStringLiteral("注册表错误码 %1").arg(code);
    }
    return message;
}

LONG openRunKey(REGSAM access, bool createIfMissing, HKEY* key)
{
    if (key == nullptr) {
        return ERROR_INVALID_PARAMETER;
    }
    LONG result = ::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, access, key);
    if (result == ERROR_FILE_NOT_FOUND && createIfMissing) {
        // Run 键理论上总是存在；极端情况下（被清理过的用户配置）按需创建。
        result = ::RegCreateKeyExW(HKEY_CURRENT_USER,
                                   kRunKeyPath,
                                   0,
                                   nullptr,
                                   0,
                                   access,
                                   nullptr,
                                   key,
                                   nullptr);
    }
    return result;
}

QString normalizedExecutablePath(const QString& command)
{
    QString path = command.trimmed();
    if (path.size() >= 2 && path.startsWith(QLatin1Char('"')) && path.endsWith(QLatin1Char('"'))) {
        path = path.mid(1, path.size() - 2);
    }
    return QDir::fromNativeSeparators(path.trimmed());
}

}  // namespace

bool setAutoStartEnabled(bool enabled, QString* errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    RegistryKey key;
    if (!enabled) {
        const LONG openResult = openRunKey(KEY_SET_VALUE, false, key.address());
        if (openResult == ERROR_FILE_NOT_FOUND || openResult == ERROR_PATH_NOT_FOUND) {
            // 键不存在 = 本来就没有自启，删除操作视为成功（幂等）。
            return true;
        }
        if (openResult != ERROR_SUCCESS) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    QStringLiteral("无法打开开机自启注册表项：%1").arg(win32ErrorMessage(openResult));
            }
            return false;
        }

        const LONG deleteResult = ::RegDeleteValueW(key.get(), kAutoStartValueName);
        if (deleteResult == ERROR_SUCCESS || deleteResult == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        if (errorMessage != nullptr) {
            *errorMessage =
                QStringLiteral("取消开机自启失败：%1").arg(win32ErrorMessage(deleteResult));
        }
        return false;
    }

    const LONG openResult = openRunKey(KEY_SET_VALUE, true, key.address());
    if (openResult != ERROR_SUCCESS) {
        if (errorMessage != nullptr) {
            *errorMessage =
                QStringLiteral("无法打开开机自启注册表项：%1").arg(win32ErrorMessage(openResult));
        }
        return false;
    }

    const QString executablePath = QCoreApplication::applicationFilePath();
    if (executablePath.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法获取当前程序路径，不能启用开机自启");
        }
        return false;
    }

    // 路径带引号，保证安装路径含空格时命令行仍能正确解析。
    const QString command =
        QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(executablePath));
    const std::wstring wideCommand = command.toStdWString();
    const DWORD bytes = static_cast<DWORD>((wideCommand.size() + 1) * sizeof(wchar_t));
    const LONG setResult = ::RegSetValueExW(key.get(),
                                            kAutoStartValueName,
                                            0,
                                            REG_SZ,
                                            reinterpret_cast<const BYTE*>(wideCommand.c_str()),
                                            bytes);
    if (setResult != ERROR_SUCCESS) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("写入开机自启失败：%1").arg(win32ErrorMessage(setResult));
        }
        return false;
    }
    qCInfo(lcStartup) << "已启用开机自启";
    return true;
}

bool isAutoStartEnabled()
{
    RegistryKey key;
    const LONG openResult = ::RegOpenKeyExW(HKEY_CURRENT_USER,
                                            kRunKeyPath,
                                            0,
                                            KEY_QUERY_VALUE,
                                            key.address());
    if (openResult != ERROR_SUCCESS) {
        return false;
    }

    DWORD valueType = 0;
    DWORD size = 0;
    LONG queryResult =
        ::RegQueryValueExW(key.get(), kAutoStartValueName, nullptr, &valueType, nullptr, &size);
    if (queryResult != ERROR_SUCCESS || size == 0) {
        return false;
    }

    QByteArray buffer(static_cast<int>(size), '\0');
    queryResult = ::RegQueryValueExW(key.get(),
                                     kAutoStartValueName,
                                     nullptr,
                                     &valueType,
                                     reinterpret_cast<LPBYTE>(buffer.data()),
                                     &size);
    if (queryResult != ERROR_SUCCESS || size == 0) {
        return false;
    }

    QString command;
    if (valueType == REG_SZ || valueType == REG_EXPAND_SZ) {
        command = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(buffer.constData()),
                                          static_cast<int>(size / sizeof(wchar_t)));
        const qsizetype terminator = command.indexOf(QChar(0));
        if (terminator >= 0) {
            command.truncate(terminator);
        }
        command = command.trimmed();
    }

    // 只要值存在就视为已启用（用户可能移动过程序目录）；路径不同仅提示，不改变结论。
    if (QCoreApplication::instance() != nullptr && !command.isEmpty()) {
        const QString storedPath = normalizedExecutablePath(command);
        const QString currentPath =
            QDir::fromNativeSeparators(QCoreApplication::applicationFilePath());
        if (!currentPath.isEmpty() && !storedPath.isEmpty()
            && storedPath.compare(currentPath, Qt::CaseInsensitive) != 0) {
            qCInfo(lcStartup) << "开机自启项存在，但指向的路径与当前程序不同，仍视为已启用";
        }
    }
    return true;
}

}  // namespace snapocr::platform::win32
