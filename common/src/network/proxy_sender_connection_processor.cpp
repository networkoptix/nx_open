#include "proxy_sender_connection_processor.h"
#include "network/universal_request_processor_p.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "utils/common/log.h"
#include "universal_tcp_listener.h"

#include <QtCore/QElapsedTimer>

#include <common/common_globals.h>


static const int SOCKET_TIMEOUT = 1000 * 5;
static const int PROXY_KEEP_ALIVE_INTERVAL = 60 * 1000;

class QnProxySenderConnectionPrivate: public QnUniversalRequestProcessorPrivate
{
public:
    SocketAddress proxyServerUrl;
    QString guid;
    int securityCode;
};

QnProxySenderConnection::QnProxySenderConnection(
        const SocketAddress& proxyServerUrl, const QString& guid, int securityCode,
        QnUniversalTcpListener* owner)
    : QnUniversalRequestProcessor(new QnProxySenderConnectionPrivate,
        QSharedPointer<AbstractStreamSocket>(SocketFactory::createStreamSocket()), owner, false)
{
    Q_D(QnProxySenderConnection);
    d->proxyServerUrl = proxyServerUrl;
    d->guid = guid;
    d->securityCode = securityCode;
    setObjectName( lit("QnProxySenderConnection") );
}

QnProxySenderConnection::~QnProxySenderConnection()
{
    stop();
}

QByteArray QnProxySenderConnection::readProxyResponse()
{
    Q_D(QnProxySenderConnection);

    quint8 buffer[1024];
    size_t bufLen = 0;
    while (d->socket->isConnected() && !m_needStop && bufLen < sizeof(buffer))
    {
        int readed = d->socket->recv(buffer + bufLen, sizeof(buffer) - static_cast<unsigned int>(bufLen));
        if (readed < 1)
            return QByteArray();
        bufLen += readed;
        QByteArray result = QByteArray::fromRawData((const char*)buffer, static_cast<int>(bufLen));
        if (QnTCPConnectionProcessor::isFullMessage(result))
            return result;
    }

    return QByteArray();
}

void QnProxySenderConnection::addNewProxyConnect()
{
    Q_D(QnProxySenderConnection);
    d->owner->addProxySenderConnections(d->proxyServerUrl, d->securityCode, 1);
}

void QnProxySenderConnection::doDelay()
{
    for (int i = 0; i < 100; ++i)
    {
        if (m_needStop)
            break;
        msleep(100);
    }
}
int QnProxySenderConnection::sendRequest(const QByteArray& data)
{
    Q_D(QnProxySenderConnection);

    int totalSend = 0;
    while (!m_needStop && d->socket->isConnected() && totalSend < data.length())
    {
        int sended = d->socket->send(data.mid(totalSend));
        if (sended <= 0)
            return sended;
        totalSend += sended;
    }
    return totalSend;
}

void QnProxySenderConnection::run()
{
    Q_D(QnProxySenderConnection);

    initSystemThreadId();

    if (!d->socket->connect(d->proxyServerUrl, SOCKET_TIMEOUT))
        return;

    d->socket->setSendTimeout(SOCKET_TIMEOUT);
    d->socket->setRecvTimeout(SOCKET_TIMEOUT);

    auto proxyRequest = QString(lit(
        "CONNECT /proxy-reverse HTTP/1.1\r\n" \
        "X-Server-Uuid: %1\r\n"
        "X-Security-Code: %2\r\n"
        "\r\n")).arg(d->guid).arg(d->securityCode).toUtf8();

    int sended = sendRequest(proxyRequest);
    if (sended < proxyRequest.length())
    {
        NX_LOG(lit("QnProxySenderConnection: can not connect to %1")
               .arg(d->proxyServerUrl.toString()), cl_logWARNING);
        return;
    }

    QByteArray response = readProxyResponse();
    if (response.isEmpty())
    {
        NX_LOG(lit("QnProxySenderConnection: no response from %1")
               .arg(d->proxyServerUrl.toString()), cl_logWARNING);
        return;
    }

    // wait main request from remote host
    bool gotRequest = false;
    QElapsedTimer timer;
    timer.restart();
    while (!m_needStop && d->socket->isConnected())
    {
        gotRequest = readRequest();
        if (gotRequest)
        {
            timer.restart();
            if (d->clientRequest.startsWith("HTTP 200 OK"))
                gotRequest = false; // proxy keep-alive packets
            else
                break;
        }
        else {
            if (timer.elapsed() > PROXY_KEEP_ALIVE_INTERVAL)
                break;
        }
    }

    if (!gotRequest && timer.elapsed() < PROXY_KEEP_ALIVE_INTERVAL)
        doDelay();

    if (!m_needStop) {
        addNewProxyConnect();
        if (gotRequest)
        {
            parseRequest();
            processRequest();
        }
    }
}
