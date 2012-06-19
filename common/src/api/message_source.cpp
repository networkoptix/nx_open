#include <QtNetwork>
#include <QtEndian>
#include "message_source.h"
#include "api/app_server_connection.h"
#include <utils/common/warnings.h>
#include "message.pb.h"

#define QN_EVENT_SOURCE_DEBUG

/** 
 * This interval (2T) has the following semantic:
 *   - Server sends us pings every T msecs
 *   - We wake up every 2T msecs
 *   - If T msecs passed sinse last received ping => emit connectionClosed.
 */
static const int PING_INTERVAL = 60000;

// -------------------------------------------------------------------------- //
// QbPbStreamParser
// -------------------------------------------------------------------------- //
class QnPbStreamParser
{
public:
    void addData(const QByteArray& data);
    bool nextMessage(pb::Message& parsed);

private:
    QQueue<QByteArray> blocks;
    QByteArray incomplete;
};

void QnPbStreamParser::addData(const QByteArray& data)
{
    incomplete += data;

    qint32 messageSize;

    while(incomplete.length() >= 4 && (messageSize = qFromBigEndian<qint32>(reinterpret_cast<const uchar*>(incomplete.constData()))) <= incomplete.length() - 4)
    {
        blocks.append(QByteArray(incomplete.data() + 4, messageSize));
        incomplete = incomplete.right(incomplete.length() - messageSize - 4);
    }
}

bool QnPbStreamParser::nextMessage(pb::Message& parsed)
{
    if (blocks.isEmpty())
        return false;

    QByteArray block = blocks.dequeue();

    if (!parsed.ParseFromArray(block.data(), block.size()))
    {
        qnWarning("Error parsing PB block: %1.", block);
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------- //
// QnJsonStreamParser
// -------------------------------------------------------------------------- //
class QnJsonStreamParser
{
public:
    void addData(const QByteArray& data);
    bool nextMessage(QVariant& parsed);

private:
    QQueue<QByteArray> blocks;
    QByteArray incomplete;

    QJson::Parser parser;
};

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


// -------------------------------------------------------------------------- //
// QnMessageSource
// -------------------------------------------------------------------------- //
QnMessageSource::QnMessageSource(QUrl url, int retryTimeout): 
    m_url(url),
    m_retryTimeout(retryTimeout),
    m_reply(NULL),
    m_seqNumber(0),
    m_streamParser(new QnPbStreamParser())
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<QnMessage>();
        metaTypesInitialized = true;
    }

    connect(this, SIGNAL(stopped()), this, SLOT(doStop()));
    connect(&m_manager, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
            this, SLOT(slotAuthenticationRequired(QNetworkReply*,QAuthenticator*)));
    connect(&m_pingTimer, SIGNAL(timeout()), this, SLOT(onPingTimer()));

#ifndef QT_NO_OPENSSL
    connect(&m_manager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(sslErrors(QNetworkReply*,QList<QSslError>)));
#endif
}

QnMessageSource::~QnMessageSource() {
    return;
}

void QnMessageSource::stop()
{
    emit stopped();
}

void QnMessageSource::doStop()
{
    m_pingTimer.stop();

    if (m_reply)
        m_reply->abort();
}

void QnMessageSource::startRequest()
{
    m_reply = m_manager.post(QNetworkRequest(m_url), "");
    connect(m_reply, SIGNAL(finished()),
            this, SLOT(httpFinished()));
    connect(m_reply, SIGNAL(readyRead()),
            this, SLOT(httpReadyRead()));

    // Check every minute
    m_pingTimer.start(PING_INTERVAL);
}

void QnMessageSource::httpFinished()
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

void QnMessageSource::httpReadyRead()
{
    QByteArray data = m_reply->readAll().data();

	if (data.size() == 0)
	{
		qDebug() << "empty";
	}

#ifdef QN_EVENT_SOURCE_DEBUG
    qDebug() << "Event data: " << data;
#endif

    m_streamParser->addData(data);

    pb::Message parsed;
    while (m_streamParser->nextMessage(parsed))
    {
        // Restart timer for any event
        m_eventWaitTimer.restart();

        QnMessage event;
        if (!event.load(parsed))
            continue;

        if (event.eventType == pb::Message_Type_Initial)
        {
            if (m_seqNumber == 0)
            {
                // No tracking yet. Just initialize seqNumber.
                m_seqNumber = event.seqNumber;
            }
            else if (QnMessage::nextSeqNumber(m_seqNumber) != event.seqNumber)
            {
                qWarning() << "QnMessageSource::httpReadyRead(): One or more event are lost. Emitting connectionReset().";

                // Tracking is on and some events are missed and/or reconnect occured
                m_seqNumber = event.seqNumber;
                emit connectionReset();
            }

            emit connectionOpened();
        } else if (event.eventType != pb::Message_Type_Ping)
        {
            emit messageReceived(event);
        }
        // If it's ping event -> just update last event time (m_eventWaitTimer)
    }
}


void QnMessageSource::slotAuthenticationRequired(QNetworkReply*,QAuthenticator *authenticator)
{
    authenticator->setUser("xxxuser");
    authenticator->setPassword("xxxpassword");
}

void QnMessageSource::onPingTimer()
{
    if (m_eventWaitTimer.elapsed() > PING_INTERVAL)
    {
        doStop();

        emit connectionClosed("Timeout");
    }
}

#ifndef QT_NO_OPENSSL
void QnMessageSource::sslErrors(QNetworkReply*,const QList<QSslError> &errors)
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
