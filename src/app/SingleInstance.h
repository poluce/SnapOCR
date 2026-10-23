#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

namespace snapocr::app {

// 单实例：
// - 用 Win32 命名互斥体做“是否已有实例”的权威判定（比探测本地套接字可靠）；
// - 用 QLocalServer 接收后续实例转发的命令（capture / ocr / show）。
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject* parent = nullptr);
    ~SingleInstance() override;

    // 尝试成为主实例。返回 false 表示已有实例在运行（errorMessage 仅有真正错误时设置）。
    bool tryBecomePrimary(QString* errorMessage);

    // 向已运行实例发送命令；返回是否发送成功。
    bool sendToPrimary(const QString& command, int timeoutMs = 2000);

signals:
    void commandReceived(const QString& command);

private:
    void onNewConnection();

    QLocalServer* m_server = nullptr;
    QString m_serverName;
    void* m_mutex = nullptr;  // HANDLE
};

}  // namespace snapocr::app
