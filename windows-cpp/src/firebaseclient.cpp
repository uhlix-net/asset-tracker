#include "firebaseclient.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <memory>
#include <stdexcept>

namespace {

constexpr int kShortTimeoutMs = 15000;
constexpr int kTransferTimeoutMs = 120000;

[[noreturn]] void fail(const QString &message) {
    throw std::runtime_error(message.toStdString());
}

QByteArray encoded(const QString &text) {
    return QUrl::toPercentEncoding(text);
}

} // namespace

FirebaseClient::FirebaseClient(const FirebaseConfig &config)
    : m_config(config)
{}

QByteArray FirebaseClient::authUrl() const {
    const QByteArray base = m_config.endpointOverride.isEmpty()
        ? QByteArray("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword")
        : m_config.endpointOverride.toUtf8() + "/auth";
    return base + "?key=" + encoded(m_config.apiKey);
}

QByteArray FirebaseClient::documentUrl(const QString &collection, const QString &id) const {
    const QByteArray base = m_config.endpointOverride.isEmpty()
        ? QByteArray("https://firestore.googleapis.com/v1")
        : m_config.endpointOverride.toUtf8() + "/firestore";
    QByteArray url = base + "/projects/" + encoded(m_config.projectId)
        + "/databases/(default)/documents/users/" + encoded(m_uid) + "/" + encoded(collection);
    if (!id.isEmpty()) url += "/" + encoded(id);
    return url;
}

QByteArray FirebaseClient::objectsUrl() const {
    const QByteArray base = m_config.endpointOverride.isEmpty()
        ? QByteArray("https://firebasestorage.googleapis.com/v0")
        : m_config.endpointOverride.toUtf8() + "/storage";
    return base + "/b/" + encoded(m_config.bucket) + "/o";
}

QString FirebaseClient::objectName(const QString &path) const {
    return QString("users/%1/%2").arg(m_uid, path);
}

QString FirebaseClient::detail(const Reply &reply) {
    const QJsonValue message = QJsonDocument::fromJson(reply.body).object()
        .value("error").toObject().value("message");
    return message.isString() ? message.toString()
                              : QString::fromUtf8(reply.body.left(120));
}

FirebaseClient::Reply FirebaseClient::send(const QByteArray &verb, const QByteArray &encodedUrl,
                                           const QByteArray &body, const QByteArray &contentType,
                                           int timeoutMs) {
    QNetworkRequest request(QUrl::fromEncoded(encodedUrl));
    request.setTransferTimeout(timeoutMs);
    if (!contentType.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (!m_token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + m_token.toLatin1());

    std::unique_ptr<QNetworkReply> reply(m_network.sendCustomRequest(request, verb, body));
    QEventLoop loop;
    QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    Reply result;
    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    if (result.status == 0) {
        // errorString() can quote the URL, and with it the API key.
        fail(QString("Network error reaching Firebase (%1).").arg(
            QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(reply->error())));
    }
    return result;
}

void FirebaseClient::authenticate() {
    const QByteArray body = QJsonDocument(QJsonObject{
        {"email", m_config.email},
        {"password", m_config.password},
        {"returnSecureToken", true},
    }).toJson(QJsonDocument::Compact);

    m_token.clear();
    const Reply reply = send("POST", authUrl(), body, "application/json", kShortTimeoutMs);
    if (reply.status != 200)
        fail(QString("Firebase authentication failed: %1").arg(detail(reply)));

    const QJsonObject data = QJsonDocument::fromJson(reply.body).object();
    m_token = data.value("idToken").toString();
    m_uid = data.value("localId").toString();
}

QVector<FirebaseClient::Document> FirebaseClient::listDocuments(const QString &collection) {
    QVector<Document> documents;
    QByteArray pageToken;
    do {
        QByteArray url = documentUrl(collection) + "?pageSize=300";
        if (!pageToken.isEmpty()) url += "&pageToken=" + pageToken;

        const Reply reply = send("GET", url, {}, {}, kShortTimeoutMs);
        if (reply.status == 404) return documents;
        if (reply.status != 200)
            fail(QString("Firestore read failed (%1): %2").arg(reply.status).arg(detail(reply)));

        const QJsonObject page = QJsonDocument::fromJson(reply.body).object();
        const QJsonArray items = page.value("documents").toArray();
        for (const QJsonValue &item : items) {
            const QJsonObject object = item.toObject();
            Document document;
            document.id = object.value("name").toString().section('/', -1);
            document.data = object.value("fields").toObject().value("data").toObject()
                                .value("stringValue").toString();
            documents.append(document);
        }
        pageToken = encoded(page.value("nextPageToken").toString());
    } while (!pageToken.isEmpty());
    return documents;
}

QString FirebaseClient::getDocument(const QString &collection, const QString &id) {
    const Reply reply = send("GET", documentUrl(collection, id), {}, {}, kShortTimeoutMs);
    if (reply.status == 404) return QString();
    if (reply.status != 200)
        fail(QString("Firestore read failed (%1): %2").arg(reply.status).arg(detail(reply)));
    return QJsonDocument::fromJson(reply.body).object().value("fields").toObject()
        .value("data").toObject().value("stringValue").toString();
}

void FirebaseClient::setDocument(const QString &collection, const QString &id,
                                 const QString &data) {
    const QJsonObject fields{
        {"data", QJsonObject{{"stringValue", data}}},
        {"updated", QJsonObject{{"stringValue",
                                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}}},
    };
    const QByteArray body = QJsonDocument(QJsonObject{{"fields", fields}})
                                .toJson(QJsonDocument::Compact);
    const Reply reply = send("PATCH", documentUrl(collection, id), body, "application/json",
                             kShortTimeoutMs);
    if (reply.status != 200 && reply.status != 201)
        fail(QString("Firestore write failed (%1): %2").arg(reply.status).arg(detail(reply)));
}

void FirebaseClient::deleteDocument(const QString &collection, const QString &id) {
    const Reply reply = send("DELETE", documentUrl(collection, id), {}, {}, kShortTimeoutMs);
    if (reply.status != 200 && reply.status != 204 && reply.status != 404)
        fail(QString("Firestore delete failed (%1): %2").arg(reply.status).arg(detail(reply)));
}

void FirebaseClient::upload(const QString &path, const QByteArray &data) {
    const QByteArray url = objectsUrl() + "?uploadType=media&name=" + encoded(objectName(path));
    const Reply reply = send("POST", url, data, "application/octet-stream", kTransferTimeoutMs);
    if (reply.status != 200 && reply.status != 201)
        fail(QString("Storage upload failed (%1): %2").arg(reply.status).arg(detail(reply)));
}

QByteArray FirebaseClient::download(const QString &path) {
    const QByteArray url = objectsUrl() + "/" + encoded(objectName(path)) + "?alt=media";
    const Reply reply = send("GET", url, {}, {}, kTransferTimeoutMs);
    if (reply.status != 200)
        fail(QString("Storage download failed (%1) for %2").arg(reply.status).arg(path));
    return reply.body;
}

void FirebaseClient::deleteObject(const QString &path) {
    const QByteArray url = objectsUrl() + "/" + encoded(objectName(path));
    const Reply reply = send("DELETE", url, {}, {}, kShortTimeoutMs);
    if (reply.status != 200 && reply.status != 204 && reply.status != 404)
        fail(QString("Storage delete failed (%1) for %2").arg(reply.status).arg(path));
}

QStringList FirebaseClient::listObjects(const QString &prefix) {
    const QString root = objectName(QString());
    QStringList names;
    QByteArray pageToken;
    do {
        QByteArray url = objectsUrl() + "?maxResults=1000&prefix=" + encoded(root + prefix);
        if (!pageToken.isEmpty()) url += "&pageToken=" + pageToken;

        const Reply reply = send("GET", url, {}, {}, kShortTimeoutMs);
        if (reply.status != 200)
            fail(QString("Storage list failed (%1): %2").arg(reply.status).arg(detail(reply)));

        const QJsonObject page = QJsonDocument::fromJson(reply.body).object();
        const QJsonArray items = page.value("items").toArray();
        for (const QJsonValue &item : items)
            names << item.toObject().value("name").toString().mid(root.size());
        pageToken = encoded(page.value("nextPageToken").toString());
    } while (!pageToken.isEmpty());
    return names;
}
