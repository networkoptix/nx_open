#include <QtNetwork>
#include "EventSource.h"

#include <utils/common/warnings.h>

#define QN_EVENT_SOURCE_DEBUG

/* This interval (2T) has the following semantic:
    - Server sends us pings every T msecs
    - We wake up every 2T msecs
    - If T msecs passed sinse last received ping => emit connectionClosed.
*/
static const int PING_INTERVAL = 60000;

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
    dict = parsed.toMap();

    if (!dict.contains("type"))
        return false;

    eventType = dict["type"].toString();

    if (eventType != QN_EVENT_RES_CHANGE
            && eventType != QN_EVENT_RES_DELETE
			&& eventType != QN_EVENT_RES_SETPARAM
			&& eventType != QN_EVENT_RES_STATUS_CHANGE
			&& eventType != QN_EVENT_RES_DISABLED_CHANGE
            && eventType != QN_EVENT_LICENSE_CHANGE
            && eventType != QN_CAMERA_SERVER_ITEM
            && eventType != QN_EVENT_EMPTY
            && eventType != QN_EVENT_PING)
    {
        return false;
    }

    if (!dict.contains("seq_num"))
    {
        // version before 1.0.1
        // Do not use seqNumber tracking
        seqNumber = 0;
    } else {
        seqNumber = dict["seq_num"].toUInt();
    }

    if (eventType == QN_EVENT_LICENSE_CHANGE)
    {
        objectId = dict["id"].toString();
        return true;
    }

    if (eventType == QN_CAMERA_SERVER_ITEM)
    {
        // will use dict
        // need to refactor a bit
        return true;
    }

    if (eventType == QN_EVENT_EMPTY || eventType == QN_EVENT_PING)
        return true;

    if (!dict.contains("resourceId"))
        return false;

    objectId = dict["resourceId"].toString();
    if (dict.contains("parentId"))
        parentId = dict["parentId"].toString();

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

quint32 QnEvent::nextSeqNumber(quint32 seqNumber)
{
    seqNumber++;

    if (seqNumber == 0)
        seqNumber++;

    return seqNumber;
}

QnEventSource::QnEventSource(QUrl url, int retryTimeout): 
    m_url(url),
    m_retryTimeout(retryTimeout),
    m_reply(NULL),
    m_seqNumber(0)
{
    connect(this, SIGNAL(stopped()), this, SLOT(doStop()));
    connect(&m_manager, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
            this, SLOT(slotAuthenticationRequired(QNetworkReply*,QAuthenticator*)));
    connect(&m_pingTimer, SIGNAL(timeout()), this, SLOT(onPingTimer()));

#ifndef QT_NO_OPENSSL
    connect(&m_manager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(sslErrors(QNetworkReply*,QList<QSslError>)));
#endif

}

void QnEventSource::stop()
{
    emit stopped();
}

void QnEventSource::doStop()
{
    m_pingTimer.stop();

    if (m_reply)
        m_reply->abort();
}

void QnEventSource::startRequest()
{
    m_reply = m_manager.post(QNetworkRequest(m_url), "");
    connect(m_reply, SIGNAL(finished()),
            this, SLOT(httpFinished()));
    connect(m_reply, SIGNAL(readyRead()),
            this, SLOT(httpReadyRead()));

    // Check every minute
    m_pingTimer.start(PING_INTERVAL);
}

void QnEventSource::httpFinished()
{
    if (!m_reply)
        return;

    QVariant redirectionTarget = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (m_reply->error()) {
        QTimer::singleShot(m_retryTimeout, this, SLOT(startRequest()));
        emit connectionClosed(m_reply->errorString());
    } else if (!redirectionTarget.isNull()) {
        m_url = m_url.resolved(redirectionTarget.toUrl());
        m_reply->deleteLater();
        startRequest();
        return;
    } else {
        // Connection closed normally by the server. Reconnect immediately.
        QTimer::singleShot(0, this, SLOT(startRequest()));
        emit connectionClosed("OK");
    }

    m_reply->deleteLater();
    m_reply = 0;
}

void QnEventSource::httpReadyRead()
{
    QByteArray data = m_reply->readAll().data();

#ifdef QN_EVENT_SOURCE_DEBUG
    qDebug() << "Event data: " << data;
#endif

    m_streamParser.addData(data);

    QVariant parsed;
    while (m_streamParser.nextMessage(parsed))
    {
        // Restart timer for any event
        m_eventWaitTimer.restart();

        QnEvent event;
        if (!event.load(parsed))
            continue;

        // If it's ping event -> just update last event time (m_eventWaitTimer)
        if (event.eventType == QN_EVENT_EMPTY)
        {
            if (m_seqNumber == 0)
            {
                // No tracking yet. Just initialize seqNumber.
                m_seqNumber = event.seqNumber;
            }
            else if (QnEvent::nextSeqNumber(m_seqNumber) != event.seqNumber)
            {
                // Tracking is on and some events are missed and/or reconnect occured
                m_seqNumber = event.seqNumber;
                emit connectionReset();
            }

            emit connectionOpened();
        } else if (event.eventType != QN_EVENT_PING)
            emit eventReceived(event);
    }
}


void QnEventSource::slotAuthenticationRequired(QNetworkReply*,QAuthenticator *authenticator)
{
    authenticator->setUser("xxxuser");
    authenticator->setPassword("xxxpassword");
}

void QnEventSource::onPingTimer()
{
    if (m_eventWaitTimer.elapsed() > PING_INTERVAL)
    {
        doStop();

        emit connectionClosed("Timeout");
    }
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

    m_reply->ignoreSslErrors();
}
#endif
