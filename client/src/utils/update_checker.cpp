#include "update_checker.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include "version.h"

QnUpdateChecker::QnUpdateChecker(const QUrl &url, QObject *parent) :
    QObject(parent),
    m_networkAccessManager(new QNetworkAccessManager(this)),
    m_url(url)
{
}

void QnUpdateChecker::checkForUpdates() {
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(m_url));
    connect(reply, &QNetworkReply::finished, this, &QnUpdateChecker::at_networkReply_finished);
}

void QnUpdateChecker::at_networkReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
        return;

    QByteArray data = reply->readAll();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    map = map.value(lit(QN_CUSTOMIZATION_NAME)).toMap();
    QnSoftwareVersion latestVersion(map.value(lit("latest_version")).toString());
    emit updateAvailable(latestVersion);
}
