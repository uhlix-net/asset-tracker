#pragma once
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Checks the GitHub releases API for a newer version than currentVersion.
// check() fires once; the result arrives via checkFinished.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(const QString &currentVersion, QObject *parent = nullptr);
    void check();

signals:
    void checkFinished(bool updateAvailable, const QString &latestVersion,
                       const QString &releaseUrl);

private:
    void onReply(QNetworkReply *reply);
    static int compareVersions(const QString &a, const QString &b);

    QString m_currentVersion;
    QNetworkAccessManager *m_network;
};
