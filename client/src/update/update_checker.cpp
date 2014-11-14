#include "update_checker.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <utils/common/app_info.h>
#include <common/common_module.h>

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
    map = map.value(QnAppInfo::customizationName()).toMap();

    QString currentRelease = map.value(lit("current_release")).toString();
    if (currentRelease.isEmpty())
        return;

    map = map.value(lit("releases")).toMap();
    QnSoftwareVersion latestVersion(map.value(currentRelease).toString());
    QnSoftwareVersion patchVersion(map.value(qnCommon->engineVersion().toString(QnSoftwareVersion::MinorFormat)).toString());
    emit updateAvailable(latestVersion, patchVersion);
}
