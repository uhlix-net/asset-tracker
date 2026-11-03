#pragma once
#include <QByteArray>
#include <QJsonObject>
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
// users/{uid}/ and each call throws std::runtime_error on failure, with a message
// written for the user. Messages never include the API key, which travels in a
// query string.
class FirebaseClient {
public:
    explicit FirebaseClient(const FirebaseConfig &config);

    // Account calls. Each signs the client in, except sendPasswordReset.
    void authenticate();                          // Sign in with config email + password.
    void createAccount();                         // Sign up with config email + password.
    void sendPasswordReset(const QString &email);
    void changePassword(const QString &newPassword);
    void deleteAccount();                         // Removes the login itself. Signed in only.
    QString uid() const { return m_uid; }

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

    QByteArray authUrl(const char *operation) const;
    QJsonObject callAuth(const char *operation, const QJsonObject &body);
    void takeSession(const QJsonObject &reply);
    QByteArray documentUrl(const QString &collection, const QString &id = QString()) const;
    QByteArray objectsUrl() const;
    QString objectName(const QString &path) const;
    static QString detail(const Reply &reply);
    static QString authMessage(const QString &code);

    FirebaseConfig m_config;
    QNetworkAccessManager m_network;
    QString m_token;
    QString m_uid;
};
