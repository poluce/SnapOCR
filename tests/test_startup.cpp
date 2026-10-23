#include <QtTest>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QString>

#include <windows.h>

#include "platform/win32/Startup.h"

using snapocr::platform::win32::isAutoStartEnabled;
using snapocr::platform::win32::setAutoStartEnabled;

namespace {

const wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t kValueName[] = L"SnapOCR";

// 测试前后保存/恢复用户的原始自启值，确保不污染开发机与 CI 的开机自启设置。
class OriginalAutoStartValue
{
public:
    ~OriginalAutoStartValue()
    {
        restore();
    }

    void capture()
    {
        m_present = false;
        HKEY key = nullptr;
        if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key)
            != ERROR_SUCCESS) {
            return;
        }
        DWORD valueType = 0;
        DWORD size = 0;
        const LONG queryResult =
            ::RegQueryValueExW(key, kValueName, nullptr, &valueType, nullptr, &size);
        if (queryResult == ERROR_SUCCESS && size > 0) {
            m_bytes.resize(static_cast<int>(size));
            if (::RegQueryValueExW(key,
                                   kValueName,
                                   nullptr,
                                   &valueType,
                                   reinterpret_cast<LPBYTE>(m_bytes.data()),
                                   &size)
                == ERROR_SUCCESS) {
                m_type = valueType;
                m_bytes.truncate(static_cast<int>(size));
                m_present = true;
            }
        }
        ::RegCloseKey(key);
    }

    bool present() const
    {
        return m_present;
    }

private:
    void restore()
    {
        if (m_restored) {
            return;
        }
        m_restored = true;
        HKEY key = nullptr;
        if (::RegCreateKeyExW(HKEY_CURRENT_USER,
                              kRunKeyPath,
                              0,
                              nullptr,
                              0,
                              KEY_SET_VALUE,
                              nullptr,
                              &key,
                              nullptr)
            != ERROR_SUCCESS) {
            return;
        }
        if (m_present) {
            (void)::RegSetValueExW(key,
                                   kValueName,
                                   0,
                                   m_type,
                                   reinterpret_cast<const BYTE*>(m_bytes.constData()),
                                   static_cast<DWORD>(m_bytes.size()));
        } else {
            (void)::RegDeleteValueW(key, kValueName);
        }
        ::RegCloseKey(key);
    }

    bool m_present = false;
    bool m_restored = false;
    DWORD m_type = REG_SZ;
    QByteArray m_bytes;
};

QString readAutoStartCommand()
{
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key)
        != ERROR_SUCCESS) {
        return QString();
    }

    QString value;
    DWORD valueType = 0;
    DWORD size = 0;
    if (::RegQueryValueExW(key, kValueName, nullptr, &valueType, nullptr, &size) == ERROR_SUCCESS
        && size > 0) {
        QByteArray bytes(static_cast<int>(size), '\0');
        if (::RegQueryValueExW(key,
                               kValueName,
                               nullptr,
                               &valueType,
                               reinterpret_cast<LPBYTE>(bytes.data()),
                               &size)
            == ERROR_SUCCESS) {
            value = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(bytes.constData()),
                                            static_cast<int>(size / sizeof(wchar_t)));
            const qsizetype terminator = value.indexOf(QChar(0));
            if (terminator >= 0) {
                value.truncate(terminator);
            }
        }
    }
    ::RegCloseKey(key);
    return value;
}

}  // namespace

class StartupTest final : public QObject
{
    Q_OBJECT

private slots:
    void queryDoesNotCrash();
    void disableIsIdempotent();
    void enableThenDisableRoundTrip();
};

void StartupTest::queryDoesNotCrash()
{
    OriginalAutoStartValue original;
    original.capture();
    // 不假设用户系统的实际状态：未配置时必须返回 false，已配置时只要求不崩溃。
    if (!original.present()) {
        QVERIFY(!isAutoStartEnabled());
    } else {
        (void)isAutoStartEnabled();
    }
}

void StartupTest::disableIsIdempotent()
{
    OriginalAutoStartValue original;
    original.capture();

    QString error;
    QVERIFY2(setAutoStartEnabled(false, &error), qPrintable(error));
    QVERIFY(!isAutoStartEnabled());

    // 幂等：重复关闭仍应返回成功。
    error.clear();
    QVERIFY2(setAutoStartEnabled(false, &error), qPrintable(error));
    QVERIFY(!isAutoStartEnabled());
}

void StartupTest::enableThenDisableRoundTrip()
{
    OriginalAutoStartValue original;
    original.capture();

    QString error;
    if (!setAutoStartEnabled(true, &error)) {
        QSKIP(qPrintable(
            QStringLiteral("当前环境无法写入注册表，跳过自启写入测试：%1").arg(error)));
    }
    QVERIFY(isAutoStartEnabled());

    // 写入值必须是带引号的当前程序路径（含空格安装目录也能正确解析）。
    const QString command = readAutoStartCommand();
    QVERIFY2(command.startsWith(QLatin1Char('"')) && command.endsWith(QLatin1Char('"')),
             "开机自启值必须带引号");
    const QString storedPath = QDir::fromNativeSeparators(command.mid(1, command.size() - 2));
    const QString expectedPath =
        QDir::fromNativeSeparators(QCoreApplication::applicationFilePath());
    QCOMPARE(storedPath.compare(expectedPath, Qt::CaseInsensitive), 0);

    QVERIFY2(setAutoStartEnabled(false, &error), qPrintable(error));
    QVERIFY(!isAutoStartEnabled());
}

QTEST_GUILESS_MAIN(StartupTest)

#include "test_startup.moc"
