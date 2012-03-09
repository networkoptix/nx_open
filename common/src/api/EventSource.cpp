#include <QtNetwork>
#include "EventSource.h"

#define QN_EVENT_SOURCE_DEBUG

void QnJsonStreamParser::addData(const QByteArray& data)
{
    if (!incomplete.isEmpty())
    {
        incomplete += data;

        if (incomplete.endsWith('\n'))
        {
            foreach(QByteArray block, incomplete.trimmed().split('\n'))
            {
                blocks.enqueue(block);
            }
            incomplete.clear();
        }
    } else
    {
        if (data.endsWith('\n'))
        {
            foreach(QByteArray block, data.trimmed().split('\n'))
            {
                blocks.enqueue(block);
            }
        } else
        {
            incomplete = data;
        }
    }
}

bool QnJsonStreamParser::nextMessage(QVariant& parsed)
{
    if (blocks.isEmpty())
        return false;

    QByteArray block = blocks.dequeue();

    bool ok;
    parsed = parser.parse(block, &ok);

    if (!ok)
        qnWarning("Error parsing JSON block: %1.", block);

    return ok;
}

QString QnEvent::objectNameLower() const
{
	if (objectName.isEmpty())
		return objectName;

	return objectName[0].toLower() + objectName.mid(1);
}

bool QnEvent::load(const QVariant& parsed)
{   
    QMap<QString, QVariant> dict = parsed.toMap();

    if (!dict.contains("type"))
        return false;

    eventType = dict["type"].toString();

    if (eventType != QN_EVENT_RES_CHANGE && eventType != QN_EVENT_RES_DELETE
            && eventType != QN_EVENT_RES_SETPARAM && eventType != QN_EVENT_RES_STATUS_CHANGE && eventType != QN_EVENT_LICENSE_CHANGE)
    {
        return false;
    }

    if (eventType == QN_EVENT_LICENSE_CHANGE)
    {
        objectId = dict["id"].toString();
        return true;
    }

    if (!dict.contains("resourceId"))
        return false;

    objectId = dict["resourceId"].toString();
    objectName = dict["objectName"].toString();

    if (dict.contains("resourceGuid"))
        resourceGuid = dict["resourceGuid"].toString();

    if (dict.contains("data"))
        data = dict["data"].toString();

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
    connect(this, SIGNAL(stopSignal()), this, SLOT(doStop()));
    connect(&qnam, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
            this, SLOT(slotAuthenticationRequired(QNetworkReply*,QAuthenticator*)));
#ifndef QT_NO_OPENSSL
    connect(&qnam, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(sslErrors(QNetworkReply*,QList<QSslError>)));
#endif

}

void QnEventSource::stop()
{
    emit stopSignal();
}

void QnEventSource::doStop()
{
    if (reply)
        reply->abort();
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
    if (!reply)
        return;

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

#ifdef QN_EVENT_SOURCE_DEBUG
    qDebug() << "Event data: " << data;
#endif

    streamParser.addData(data);

    QVariant parsed;
    while (streamParser.nextMessage(parsed))
    {
        QnEvent event;
        event.load(parsed);

        emit eventReceived(event);
    }
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
    qnWarning("SSL errors: %1.", errorString);

    reply->ignoreSslErrors();
}
#endif
