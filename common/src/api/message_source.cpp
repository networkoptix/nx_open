#include <QtNetwork>
#include <QtEndian>
#include "message_source.h"
#include "api/app_server_connection.h"
#include <utils/common/warnings.h>
#include "message.pb.h"
#include "common/common_meta_types.h"

#define QN_EVENT_SOURCE_DEBUG

/** 
 * This interval (2T) has the following semantic:
 *   - Server sends us pings every T msecs
 *   - We wake up every 2T msecs
 *   - If 2T msecs passed sinse last received ping => emit connectionClosed.
 */
static const int PING_INTERVAL = 60000;

// -------------------------------------------------------------------------- //
// QbPbStreamParser
// -------------------------------------------------------------------------- //
class QnPbStreamParser
{
public:
    void reset();
    void addData(const QByteArray& data);
    bool nextMessage(pb::Message& parsed);

private:
    QQueue<QByteArray> blocks;
    QByteArray incomplete;
};

void QnPbStreamParser::reset()
{
    incomplete.clear();
}

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
// QnMessageSource
// -------------------------------------------------------------------------- //
QnMessageSource::QnMessageSource(QUrl url, int retryTimeout): 
    m_timeoutFlag(false),
    m_url(url),
    m_retryTimeout(retryTimeout),
    m_reply(NULL),
    m_seqNumber(0),
    m_streamParser(new QnPbStreamParser())
{
    QnCommonMetaTypes::initilize();

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
    m_streamParser->reset();

    m_timeoutFlag = false;
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
    if (QNetworkReply::NetworkError err = m_reply->error()) {

        /** Operation was cancelled intentionally */
        if (err != QNetworkReply::OperationCanceledError || m_timeoutFlag)
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
        emit connectionClosed(QLatin1String("OK"));
    }

    m_reply->deleteLater();
    m_reply = 0;
}

void QnMessageSource::httpReadyRead()
{
    QByteArray data = m_reply->readAll();

    m_streamParser->addData(data);

    pb::Message parsed;
    while (m_streamParser->nextMessage(parsed))
    {
        // Restart timer for any event
        m_eventWaitTimer.restart();

        QnMessage event;
        if (!event.load(parsed))
            continue;

        if (event.eventType == Qn::Message_Type_Initial)
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
        } else if (event.eventType != Qn::Message_Type_Ping)
        {
            emit messageReceived(event);
        }
        // If it's ping event -> just update last event time (m_eventWaitTimer)
    }
}


void QnMessageSource::slotAuthenticationRequired(QNetworkReply*,QAuthenticator *authenticator)
{
    authenticator->setUser(QLatin1String("xxxuser"));
    authenticator->setPassword(QLatin1String("xxxpassword"));
}

void QnMessageSource::onPingTimer()
{
    if (m_eventWaitTimer.elapsed() > 2 * PING_INTERVAL)
    {
        m_timeoutFlag = true;
        doStop();

        emit connectionClosed(QLatin1String("Timeout"));
    }
}

#ifndef QT_NO_OPENSSL
void QnMessageSource::sslErrors(QNetworkReply*,const QList<QSslError> &errors)
{
    QString errorString;
    foreach (const QSslError &error, errors) {
        if (!errorString.isEmpty())
            errorString += QLatin1String(", ");
        errorString += error.errorString();
    }
    qnWarning("SSL errors: %1.", errorString);

    m_reply->ignoreSslErrors();
}
#endif
