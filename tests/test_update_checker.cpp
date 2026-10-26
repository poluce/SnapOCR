#include <QtTest>

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "update/UpdateChecker.h"

using namespace snapocr::update;

// 更新检查端到端（离线）：用本地 HTTP 服务模拟发布源，验证真实网络路径、
// JSON 解析、版本比较与错误处理（F-SYS-16）。
class UpdateCheckerTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void detectsNewerVersion();
    void ignoresSameAndOlderVersion();
    void reportsHttp404();
    void rejectsNonHttpsFeed();
    void rejectsMalformedPayload();

private:
    struct CheckResult
    {
        bool done = false;
        bool ok = false;
        bool newer = false;
        UpdateRelease release;
        QString error;
    };

    QUrl startServer(const QByteArray& body, int statusCode);
    CheckResult runCheck(UpdateChecker& checker);

    QTcpServer* m_server = nullptr;
    QNetworkAccessManager* m_network = nullptr;
};

void UpdateCheckerTest::init()
{
    m_server = new QTcpServer(this);
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
    m_network = new QNetworkAccessManager(this);

    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket* socket = m_server->nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                socket->readAll();
                const QByteArray body = socket->property("body").toByteArray();
                const int status = socket->property("status").toInt();
                const QByteArray reason = status == 200 ? "OK" : "Not Found";
                QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason
                                      + "\r\nContent-Type: application/json\r\n"
                                        "Content-Length: "
                                      + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n"
                                      + body;
                socket->write(response);
                socket->disconnectFromHost();
            });
            socket->setProperty("body", m_server->property("body"));
            socket->setProperty("status", m_server->property("status"));
        }
    });
}

void UpdateCheckerTest::cleanup()
{
    if (m_server != nullptr) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

UpdateCheckerTest::CheckResult UpdateCheckerTest::runCheck(UpdateChecker& checker)
{
    CheckResult result;
    QEventLoop loop;
    connect(&checker, &UpdateChecker::finished, &loop,
            [&result, &loop](bool ok, const UpdateRelease& release, bool newer, const QString& error) {
                result.done = true;
                result.ok = ok;
                result.newer = newer;
                result.release = release;
                result.error = error;
                loop.quit();
            });
    checker.checkAsync();
    if (!result.done) {
        QTimer::singleShot(8000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return result;
}

QUrl UpdateCheckerTest::startServer(const QByteArray& body, int statusCode)
{
    m_server->setProperty("body", body);
    m_server->setProperty("status", statusCode);
    return QUrl(QStringLiteral("http://127.0.0.1:%1/latest.json").arg(m_server->serverPort()));
}

void UpdateCheckerTest::detectsNewerVersion()
{
    const QByteArray body = R"({
      "version": "0.3.0",
      "notes": "new",
      "setupUrl": "http://127.0.0.1/SnapOCR-0.3.0-setup.exe",
      "setupSize": 123
    })";
    UpdateChecker checker;
    checker.setNetworkManager(m_network);
    checker.setCurrentVersion(QStringLiteral("0.2.0"));
    checker.setFeedUrl(startServer(body, 200));

    const CheckResult result = runCheck(checker);
    QVERIFY(result.done);
    QVERIFY(result.ok);
    QCOMPARE(result.release.version, QStringLiteral("0.3.0"));
    QVERIFY(result.newer);
    QVERIFY(result.error.isEmpty());
}

void UpdateCheckerTest::ignoresSameAndOlderVersion()
{
    UpdateChecker checker;
    checker.setNetworkManager(m_network);
    checker.setCurrentVersion(QStringLiteral("0.3.0"));
    checker.setFeedUrl(startServer(R"({"version":"0.3.0"})", 200));
    const CheckResult same = runCheck(checker);
    QVERIFY(same.done);
    QVERIFY(same.ok);
    QVERIFY(!same.newer);

    checker.setFeedUrl(startServer(R"({"version":"0.2.9"})", 200));
    const CheckResult older = runCheck(checker);
    QVERIFY(older.done);
    QVERIFY(older.ok);
    QVERIFY(!older.newer);
}

void UpdateCheckerTest::reportsHttp404()
{
    UpdateChecker checker;
    checker.setNetworkManager(m_network);
    checker.setCurrentVersion(QStringLiteral("0.2.0"));
    checker.setFeedUrl(startServer("Not Found", 404));
    const CheckResult result = runCheck(checker);
    QVERIFY(result.done);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("404")));
}

void UpdateCheckerTest::rejectsNonHttpsFeed()
{
    UpdateChecker checker;
    checker.setCurrentVersion(QStringLiteral("0.2.0"));
    checker.setFeedUrl(QUrl(QStringLiteral("ftp://example.com/latest.json")));
    const CheckResult result = runCheck(checker);
    QVERIFY(result.done);
    QVERIFY(!result.ok);
}

void UpdateCheckerTest::rejectsMalformedPayload()
{
    UpdateChecker checker;
    checker.setNetworkManager(m_network);
    checker.setCurrentVersion(QStringLiteral("0.2.0"));
    checker.setFeedUrl(startServer("<html>oops</html>", 200));
    const CheckResult result = runCheck(checker);
    QVERIFY(result.done);
    QVERIFY(!result.ok);
}

QTEST_GUILESS_MAIN(UpdateCheckerTest)

#include "test_update_checker.moc"
