#include "online_help_detector.h"

#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QFile>

#include <utils/common/log.h>
#include <utils/common/app_info.h>

namespace {
const int maxUrlLength = 400;
}

QnOnlineHelpDetector::QnOnlineHelpDetector(QObject *parent) :
    QObject(parent),
    m_networkAccessManager(new QNetworkAccessManager(this))
{
    fetchHelpUrl();
}

void QnOnlineHelpDetector::fetchHelpUrl() {
    //TODO: #dklychkov 2.4 fix networkAccessManager to  nx_http::AsyncHttpClient
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(QUrl::fromUserInput(QnAppInfo::helpUrl())));
    connect(reply, &QNetworkReply::finished, this, &QnOnlineHelpDetector::at_networkReply_finished);
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(at_networkReply_error()));
}

void QnOnlineHelpDetector::at_networkReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
        return;

    QByteArray data = reply->readAll();
    QString url = QString::fromUtf8(data).trimmed();
    if (url.size() < maxUrlLength && QUrl::fromUserInput(url).isValid()) {
        emit urlFetched(url);
        cl_log.log("Online help url has been fetched: ", url, cl_logINFO);
    } else {
        emit error();
        cl_log.log("Wrong online help url has been fetched: ", url, cl_logWARNING);
    }
}

void QnOnlineHelpDetector::at_networkReply_error() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    emit error();
    cl_log.log("Can't fetch online help URL", cl_logWARNING);
}
