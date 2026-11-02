#include "updatechecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace {
const char kReleasesUrl[] = "https://api.github.com/repos/uhlix-net/asset-tracker/releases/latest";
}

UpdateChecker::UpdateChecker(const QString &currentVersion, QObject *parent)
    : QObject(parent)
    , m_currentVersion(currentVersion)
    , m_network(new QNetworkAccessManager(this))
{
    connect(m_network, &QNetworkAccessManager::finished, this, &UpdateChecker::onReply);
}

void UpdateChecker::check() {
    QNetworkRequest request{QUrl(QString::fromLatin1(kReleasesUrl))};
    request.setRawHeader("User-Agent", "asset-tracker-update-check");
    m_network->get(request);
}

void UpdateChecker::onReply(QNetworkReply *reply) {
    reply->deleteLater();
    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    const QString tag = release.value("tag_name").toString();
    if (reply->error() != QNetworkReply::NoError || tag.isEmpty()) {
        emit checkFinished(false, QString(), QString());
        return;
    }
    emit checkFinished(compareVersions(tag, m_currentVersion) > 0, tag,
                       release.value("html_url").toString());
}

// Returns >0 if a is newer than b, 0 if equal, <0 if older.
int UpdateChecker::compareVersions(const QString &a, const QString &b) {
    const auto parts = [](const QString &version) {
        return QString(version).remove(QRegularExpression("^[vV]")).split('.');
    };
    const QStringList pa = parts(a), pb = parts(b);
    const int count = qMax(pa.size(), pb.size());
    for (int i = 0; i < count; ++i) {
        const int va = i < pa.size() ? pa[i].toInt() : 0;
        const int vb = i < pb.size() ? pb[i].toInt() : 0;
        if (va != vb) return va - vb;
    }
    return 0;
}
