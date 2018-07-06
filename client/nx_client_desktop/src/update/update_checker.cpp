#include "update_checker.h"

#include <QtCore/QJsonDocument>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <common/static_common_module.h>

#include <update/update_info.h>

#include <utils/common/app_info.h>

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
    if (qnStaticCommon->engineVersion() > QnSoftwareVersion(currentRelease))
        currentRelease = qnStaticCommon->engineVersion().toString(QnSoftwareVersion::MinorFormat);

    if (currentRelease.isEmpty())
        return;

    QnUpdateInfo info;
    info.releaseNotesUrl = nx::utils::Url::fromQUrl(map.value(lit("release_notes")).toUrl());
    info.releaseDateMs =  map.value(lit("release_date")).toLongLong();
    info.releaseDeliveryDays = map.value(lit("release_delivery")).toInt();
    info.description = map.value(lit("description")).toString();

    QVariantMap releases = map.value(lit("releases")).toMap();
    info.currentRelease = QnSoftwareVersion(releases.value(currentRelease).toString());
    if (!info.currentRelease.isNull())
        emit updateAvailable(info);
}
