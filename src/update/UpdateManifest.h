#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QUrl>

namespace snapocr::update {

// 一次可用的更新（解析自 GitHub Releases API 或自定义 manifest）。
struct UpdateRelease
{
    QString version;   // 规范化版本号（去掉 v 前缀），如 0.3.0
    QString tagName;   // 原始 tag，如 v0.3.0
    QString notes;     // 更新说明（纯文本，展示时截断）
    QString htmlUrl;   // 发布页（供用户手动查看）
    QUrl setupUrl;     // 安装包下载地址（优先）
    QString setupName; // 安装包文件名
    qint64 setupSize = 0;
    QUrl zipUrl;       // 绿色包下载地址（回退）
    qint64 zipSize = 0;

    bool isEmpty() const { return version.isEmpty(); }
    bool hasInstaller() const { return setupUrl.isValid() && !setupUrl.isEmpty(); }
};

// 版本号规范化：去掉首尾空白与可选的前缀 'v'/'V'。
QString normalizeVersion(const QString& tag);

// 语义化比较：a < b 返回 -1，相等 0，a > b 返回 1。
// 支持 1.2.3 / 1.2 / 1.2.3-rc.1；预发布版本低于同号正式版。
int compareVersions(const QString& a, const QString& b);

// 解析更新源响应体：
// 1) GitHub Releases API（含 tag_name / assets / body / html_url）；
// 2) 自定义 manifest（{"version","setupUrl","setupSize","zipUrl","notes","htmlUrl"}）。
// 失败返回 false 并填充 errorMessage（消息可直接展示给用户）。
bool parseUpdatePayload(const QByteArray& payload, UpdateRelease* release,
                        QString* errorMessage);

}  // namespace snapocr::update

Q_DECLARE_METATYPE(snapocr::update::UpdateRelease)
