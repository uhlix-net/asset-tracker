#pragma once
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QString>
#include <QStringList>
#include <QVector>

struct FirebaseConfig {
    QString apiKey;
    QString projectId;
    QString bucket;
    QString email;
    QString password;
    QString endpointOverride;   // Tests only: a local stand-in for all three Google hosts.
};

// Blocking Firebase REST calls for use on a worker thread. Everything lives under
// users/{uid}/ and each call throws std::runtime_error on failure. Messages never
// include the API key, which travels in a query string.
class FirebaseClient {
public:
    explicit FirebaseClient(const FirebaseConfig &config);

    void authenticate();

    struct Document {
        QString id;
        QString data;   // The base64 AES-GCM payload.
    };
    QVector<Document> listDocuments(const QString &collection);
    QString getDocument(const QString &collection, const QString &id);  // Empty if missing.
    void setDocument(const QString &collection, const QString &id, const QString &data);
    void deleteDocument(const QString &collection, const QString &id);

    // Object paths are relative to users/{uid}/, e.g. "files/00001/photo.jpg".
    void upload(const QString &path, const QByteArray &data);
    QByteArray download(const QString &path);
    void deleteObject(const QString &path);
    QStringList listObjects(const QString &prefix);

private:
    struct Reply {
        int status = 0;
        QByteArray body;
    };
    Reply send(const QByteArray &verb, const QByteArray &encodedUrl, const QByteArray &body,
               const QByteArray &contentType, int timeoutMs);

    QByteArray authUrl() const;
    QByteArray documentUrl(const QString &collection, const QString &id = QString()) const;
    QByteArray objectsUrl() const;
    QString objectName(const QString &path) const;
    static QString detail(const Reply &reply);

    FirebaseConfig m_config;
    QNetworkAccessManager m_network;
    QString m_token;
    QString m_uid;
};
