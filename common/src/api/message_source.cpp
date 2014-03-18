#include <QtNetwork/QtNetwork>
#include <QtCore/QtEndian>
#include "message_source.h"
#include "api/app_server_connection.h"
#include <utils/common/warnings.h>
#include "message.pb.h"

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

void QnMessageSource::setAuthKey(const QString& authKey)
{
    m_authKey = authKey;
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

    QNetworkRequest request(m_url);
    request.setRawHeader("Authorization", "Basic " + m_url.userInfo().toLatin1().toBase64());
    if (!m_authKey.isEmpty())
        request.setRawHeader("X-NetworkOptix-AuthKey", m_authKey.toLatin1());

    m_reply = m_manager.post(request, "");
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
        emit connectionClosed(m_reply->errorString());

        /** Operation was cancelled intentionally */
        if (m_timeoutFlag) {
            m_seqNumber = -1;
            m_reply->deleteLater();
            startRequest();
            return;
        }
        else if (err != QNetworkReply::OperationCanceledError) {
            QTimer::singleShot(m_retryTimeout, this, SLOT(startRequest()));
        }

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

        QnMessage message;
        if (!message.load(parsed))
            continue;

        if (message.messageType == Qn::Message_Type_Initial)
        {
            if (m_seqNumber == 0)
            {
                // No tracking yet. Just initialize seqNumber.
                m_seqNumber = message.seqNumber;
            }
            else if (QnMessage::nextSeqNumber(m_seqNumber) != message.seqNumber)
            {
                qWarning() << "QnMessageSource::httpReadyRead(): One or more event are lost. Emitting connectionReset(). Expected: " 
                    << QnMessage::nextSeqNumber(m_seqNumber) << ". Got: " << message.seqNumber;

                // Tracking is on and some events are missed and/or reconnect occured
                m_seqNumber = message.seqNumber;
                emit connectionReset();
            }

            emit connectionOpened(message);
        }

        // If it's ping event -> just update last event time (m_eventWaitTimer)
        if (message.messageType != Qn::Message_Type_Ping)
            emit messageReceived(message);

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
