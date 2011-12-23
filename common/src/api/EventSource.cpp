#include <QtNetwork>

#include "EventSource.h"

bool QnEvent::parse(const QByteArray& rawData)
{
    QJson::Parser parser;
    bool ok;
    QVariant parsed = parser.parse(rawData, &ok);

    if (!ok)
        return false;

    QMap<QString, QVariant> dict = parsed.toMap();

    if (!dict.contains("type"))
        return false;

    eventType = dict["type"].toString();

    if (eventType != QN_EVENT_RES_CHANGE && eventType != QN_EVENT_RES_DELETE && eventType != QN_EVENT_RES_SETPARAM)
    {
        return false;
    }

    if (!dict.contains("resourceId"))
        return false;

    resourceId = dict["resourceId"].toString();
    objectName = dict["objectName"].toString();

    if (eventType == QN_EVENT_RES_SETPARAM)
    {
        if (!dict.contains("paramName") || !dict.contains("paramValue"))
            return false;

        paramName = dict["paramName"].toString();
        paramValue = dict["paramValue"].toString();

        return true;
    }

    return true;
}

QnEventSource::QnEventSource(QUrl url, int retryTimeout)
    : m_url(url),
      m_retryTimeout(retryTimeout)
{
    connect(&qnam, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
            this, SLOT(slotAuthenticationRequired(QNetworkReply*,QAuthenticator*)));
#ifndef QT_NO_OPENSSL
    connect(&qnam, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(sslErrors(QNetworkReply*,QList<QSslError>)));
#endif

}

void QnEventSource::startRequest()
{
    reply = qnam.post(QNetworkRequest(m_url), "");
    connect(reply, SIGNAL(finished()),
            this, SLOT(httpFinished()));
    connect(reply, SIGNAL(readyRead()),
            this, SLOT(httpReadyRead()));
//    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
//            this, SLOT(slotError(QNetworkReply::NetworkError)));
}
/*
void QnEventSource::slotError(QNetworkReply::NetworkError code)
{
    reply->deleteLater();

    emit connectionAborted(reply->errorString());
}
*/
void QnEventSource::httpFinished()
{
    QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (reply->error()) {
        QTimer::singleShot(m_retryTimeout, this, SLOT(startRequest()));
        emit connectionClosed(reply->errorString());
    } else if (!redirectionTarget.isNull()) {
        m_url = m_url.resolved(redirectionTarget.toUrl());
        reply->deleteLater();
        startRequest();
        return;
    } else {
        // Connection closed normally by the server. Reconnect immediately.
        QTimer::singleShot(0, this, SLOT(startRequest()));
        emit connectionClosed("OK");
    }

    reply->deleteLater();
    reply = 0;
}

void QnEventSource::httpReadyRead()
{
    QByteArray data = reply->readAll().data();

    qDebug() << "Event: " << data;

    QnEvent event;
    if (!event.parse(data))
        qDebug() << "Can't parse event: " << data;
    else
        emit eventReceived(event);
}


void QnEventSource::slotAuthenticationRequired(QNetworkReply*,QAuthenticator *authenticator)
{
    authenticator->setUser("xxxuser");
    authenticator->setPassword("xxxpassword");
}

#ifndef QT_NO_OPENSSL
void QnEventSource::sslErrors(QNetworkReply*,const QList<QSslError> &errors)
{
    QString errorString;
    foreach (const QSslError &error, errors) {
        if (!errorString.isEmpty())
            errorString += ", ";
        errorString += error.errorString();
    }

    qDebug() << "One or more SSL errors has occurred";
    reply->ignoreSslErrors();
}
#endif
