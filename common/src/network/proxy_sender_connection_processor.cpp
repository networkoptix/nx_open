#include "proxy_sender_connection_processor.h"
#include "network/universal_request_processor_p.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "universal_tcp_listener.h"

static const int SOCKET_TIMEOUT = 1000 * 5;

class QnProxySenderConnectionPrivate: public QnUniversalRequestProcessorPrivate
{
public:
    QUrl proxyServerUrl;
    QString guid;
};

QnProxySenderConnection::QnProxySenderConnection(const QUrl& proxyServerUrl, const QString& guid, QnTcpListener* owner):
    QnUniversalRequestProcessor(new QnProxySenderConnectionPrivate, new TCPSocket(), owner)
{
    Q_D(QnProxySenderConnection);
    d->proxyServerUrl = proxyServerUrl;
    d->guid = guid;
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
    int bufLen = 0;
    while (d->socket->isConnected() && !m_needStop && bufLen < sizeof(buffer))
    {
        int readed = d->socket->recv(buffer + bufLen, sizeof(buffer) - bufLen);
        if (readed < 1)
            return QByteArray();
        bufLen += readed;
        QByteArray result = QByteArray::fromRawData((const char*)buffer, bufLen);
        if (QnTCPConnectionProcessor::isFullMessage(result))
            return result;
    }
    return QByteArray();
}

void QnProxySenderConnection::addNewProxyConnect()
{
    Q_D(QnProxySenderConnection);
    (dynamic_cast<QnUniversalTcpListener*>(d->owner))->addProxySenderConnections(1);
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


void QnProxySenderConnection::run()
{
    Q_D(QnProxySenderConnection);

    saveSysThreadID();

    if (!d->socket->connect(d->proxyServerUrl.host(), d->proxyServerUrl.port(), SOCKET_TIMEOUT)) {
        doDelay();
        addNewProxyConnect();
        return;
    }

    d->socket->setWriteTimeOut(SOCKET_TIMEOUT);
    d->socket->setReadTimeOut(SOCKET_TIMEOUT);

    QByteArray proxyRequest = QString(lit("CONNECT %1 PROXY/1.0\r\n\r\n")).arg(d->guid).toUtf8();

    // send proxy response
    int totalSend = 0;
    while (!m_needStop && d->socket->isConnected() && totalSend < proxyRequest.length())
    {
        int sended = d->socket->send(proxyRequest.mid(totalSend));
        if (sended > 0)
            totalSend += sended;
    }
    if (totalSend < proxyRequest.length()) {
        doDelay();
        addNewProxyConnect();
        return;
    }


    // read proxy response
    QByteArray response = readProxyResponse();
    if (response.isEmpty() && !response.startsWith("PROXY")) {
        doDelay();
        addNewProxyConnect();
        return;
    }

    // wait main request from remote host
    while (!m_needStop && d->socket->isConnected())
    {
        if (readRequest()) {
            processRequest();
            break;
        }
    }

    addNewProxyConnect();
}
