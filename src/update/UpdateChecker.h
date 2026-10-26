#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include "update/UpdateManifest.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace snapocr::update {

// 更新检查：向发布源发起一次 HTTPS 请求，解析版本信息并判断是否有新版。
//
// 约束：
// - 仅在显式调用 checkAsync() 时联网（启动时检查与托盘菜单「检查更新」都由上层决定）；
// - 生产环境只允许 https；测试可用回环 http（127.0.0.1/localhost）；
// - 不发送任何用户数据，只带必要的 User-Agent 与 Accept 头。
class UpdateChecker final : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker() override;

    static QUrl defaultFeedUrl();  // GitHub Releases API …/releases/latest
    static constexpr int kDefaultTimeoutMs = 10000;

    void setFeedUrl(const QUrl& url);
    QUrl feedUrl() const { return m_feedUrl; }

    void setCurrentVersion(const QString& version) { m_currentVersion = version; }
    QString currentVersion() const { return m_currentVersion; }

    // 测试注入；为空时内部自建（parent 为本对象）。
    void setNetworkManager(QNetworkAccessManager* manager);

    bool isChecking() const { return m_reply != nullptr; }
    void checkAsync();

signals:
    void finished(bool ok, const snapocr::update::UpdateRelease& release,
                  bool newerAvailable, const QString& errorMessage);

private:
    void handleReply();
    void fail(const QString& message);
    bool isAllowedUrl(const QUrl& url) const;

    QNetworkAccessManager* m_manager = nullptr;
    QNetworkAccessManager* m_ownedManager = nullptr;
    QNetworkReply* m_reply = nullptr;
    QTimer* m_timeoutTimer = nullptr;
    QUrl m_feedUrl;
    QString m_currentVersion;
};

}  // namespace snapocr::update
