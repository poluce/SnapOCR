#include "update/UpdateChecker.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include "snapocr/Version.h"

namespace snapocr::update {

QUrl UpdateChecker::defaultFeedUrl()
{
    return QUrl(QStringLiteral("https://api.github.com/repos/poluce/SnapOCR/releases/latest"));
}

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<UpdateRelease>("snapocr::update::UpdateRelease");
    m_ownedManager = new QNetworkAccessManager(this);
    m_manager = m_ownedManager;
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_reply != nullptr) {
            m_reply->abort();
        }
    });
    m_feedUrl = defaultFeedUrl();
}

UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::setFeedUrl(const QUrl& url)
{
    m_feedUrl = url;
}

void UpdateChecker::setNetworkManager(QNetworkAccessManager* manager)
{
    m_manager = manager != nullptr ? manager : m_ownedManager;
}

bool UpdateChecker::isAllowedUrl(const QUrl& url) const
{
    if (!url.isValid() || url.host().isEmpty()) {
        return false;
    }
    if (url.scheme() == QStringLiteral("https")) {
        return true;
    }
    // 仅允许回环地址走 http（单测/本地 manifest），生产源必须 https
    if (url.scheme() == QStringLiteral("http")) {
        return url.host() == QStringLiteral("127.0.0.1")
               || url.host().compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0;
    }
    return false;
}

void UpdateChecker::fail(const QString& message)
{
    m_timeoutTimer->stop();
    if (m_reply != nullptr) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    emit finished(false, UpdateRelease(), false, message);
}

void UpdateChecker::checkAsync()
{
    if (m_reply != nullptr) {
        return;  // 已有检查在进行
    }
    if (!isAllowedUrl(m_feedUrl)) {
        fail(QStringLiteral("更新源地址无效（必须为 https）"));
        return;
    }

    QNetworkRequest request(m_feedUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("SnapOCR/%1").arg(QString::fromUtf8(version::kVersion)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kDefaultTimeoutMs);

    m_reply = m_manager->get(request);
    m_timeoutTimer->start(kDefaultTimeoutMs);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::handleReply);
}

void UpdateChecker::handleReply()
{
    if (m_reply == nullptr) {
        return;
    }
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    m_timeoutTimer->stop();
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        emit finished(false, UpdateRelease(), false, QStringLiteral("检查更新超时"));
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        QString reason = reply->errorString();
        if (status == 404) {
            reason = QStringLiteral("更新源不可用（HTTP 404，可能是私有仓库或未发布）");
        } else if (status == 401 || status == 403) {
            reason = QStringLiteral("更新源拒绝访问（HTTP %1）").arg(status);
        } else if (status > 0) {
            reason = QStringLiteral("更新源返回 HTTP %1").arg(status);
        }
        emit finished(false, UpdateRelease(), false, reason);
        return;
    }
    if (status >= 400) {
        emit finished(false, UpdateRelease(), false,
                      QStringLiteral("更新源返回 HTTP %1").arg(status));
        return;
    }

    UpdateRelease release;
    QString parseError;
    if (!parseUpdatePayload(payload, &release, &parseError)) {
        emit finished(false, UpdateRelease(), false, parseError);
        return;
    }

    const int comparison = compareVersions(release.version, m_currentVersion);
    emit finished(true, release, comparison > 0, QString());
}

}  // namespace snapocr::update
