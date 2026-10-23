#include "app/SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

#include "snapocr/Version.h"

#include <windows.h>

namespace snapocr::app {

SingleInstance::SingleInstance(QObject* parent)
    : QObject(parent)
    , m_serverName(QString::fromUtf8(snapocr::version::kSingleInstanceKey))
{
}

SingleInstance::~SingleInstance()
{
    if (m_server != nullptr) {
        m_server->close();
    }
    if (m_mutex != nullptr) {
        ::CloseHandle(static_cast<HANDLE>(m_mutex));
        m_mutex = nullptr;
    }
}

bool SingleInstance::tryBecomePrimary(QString* errorMessage)
{
    // 1) 命名互斥体：句柄存在且 ERROR_ALREADY_EXISTS 说明已有实例。
    const QString mutexName = QStringLiteral("Local\\%1").arg(m_serverName);
    HANDLE mutex = ::CreateMutexW(nullptr, TRUE, reinterpret_cast<LPCWSTR>(mutexName.utf16()));
    if (mutex == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("创建单实例互斥体失败（错误码 %1）").arg(::GetLastError());
        }
        return true;  // 保守起见继续运行，不让用户打不开程序
    }
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        ::CloseHandle(mutex);
        return false;
    }
    m_mutex = mutex;
    ::ReleaseMutex(mutex);

    // 2) 本地套接字服务：接收后续实例转发的命令。
    //    先探测再清理，避免误删正在运行实例的管道名。
    QLocalSocket probe;
    probe.connectToServer(m_serverName);
    const bool someoneListening = probe.waitForConnected(200);
    probe.abort();
    if (someoneListening) {
        // 互斥体说我们是主实例，但仍有旧管道：清掉后重建。
        QLocalServer::removeServer(m_serverName);
    }

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    if (!m_server->listen(m_serverName)) {
        // 监听失败不致命：命令转发不可用，但不影响截图主功能。
        qWarning("单实例服务监听失败：%s", qUtf8Printable(m_server->errorString()));
    }
    return true;
}

bool SingleInstance::sendToPrimary(const QString& command, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    if (!socket.waitForConnected(timeoutMs)) {
        return false;
    }
    socket.write(command.toUtf8());
    socket.flush();
    return socket.waitForBytesWritten(timeoutMs);
}

void SingleInstance::onNewConnection()
{
    while (m_server != nullptr && m_server->hasPendingConnections()) {
        QLocalSocket* socket = m_server->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        auto deliver = [this, socket]() {
            const QString command = QString::fromUtf8(socket->readAll()).trimmed();
            if (!command.isEmpty()) {
                emit commandReceived(command);
            }
        };
        connect(socket, &QLocalSocket::readyRead, this, deliver);
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        // 连接已建立但数据稍后到达时兜底。
        QTimer::singleShot(200, socket, deliver);
    }
}

}  // namespace snapocr::app
