
#include "common/common_globals.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/user_resource.h"
#include "proxy_sender_connection_processor.h"
#include "network/universal_request_processor_p.h"
#include "utils/network/http/auth_tools.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "utils/common/log.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/common/synctime.h"

#include "universal_tcp_listener.h"

#include <QtCore/QElapsedTimer>


static const int SOCKET_TIMEOUT = 1000 * 5;
static const int PROXY_KEEP_ALIVE_INTERVAL = 60 * 1000;

class QnProxySenderConnectionPrivate: public QnUniversalRequestProcessorPrivate
{
public:
    SocketAddress proxyServerUrl;
    QnUuid guid;
};

QnProxySenderConnection::QnProxySenderConnection(
        const SocketAddress& proxyServerUrl, const QnUuid& guid,
        QnUniversalTcpListener* owner)
    : QnUniversalRequestProcessor(new QnProxySenderConnectionPrivate,
        QSharedPointer<AbstractStreamSocket>(SocketFactory::createStreamSocket()), owner, false)
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
    d->owner->addProxySenderConnections(d->proxyServerUrl, 1);
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

static QByteArray makeProxyRequest(const QnUuid& serverUuid, const QUrl& url)
{
    const QByteArray H_REALM("NX");
    const QByteArray H_METHOD("CONNECT");
    const QByteArray H_PATH("/proxy-reverse");
    const QByteArray H_AUTH("auth-int");

    const auto admin = qnResPool->getAdministrator();
    const auto time = qnSyncTime->currentUSecsSinceEpoch();

    nx_http::header::WWWAuthenticate authHeader;
    authHeader.authScheme = nx_http::header::AuthScheme::digest;
    authHeader.params["nonce"] = QString::number(time, 16).toLatin1();
    authHeader.params["realm"] = H_REALM;

    nx_http::header::DigestAuthorization digestHeader;
    if (!nx_http::calcDigestResponse(
                H_METHOD, admin->getName().toUtf8(), boost::none, admin->getDigest(),
                url, authHeader, &digestHeader))
        return QByteArray();

    return QString(QLatin1String(
       "%1 %2 HTTP/1.1\r\n" \
       "Host: %3\r\n" \
       "Authorization: %4\r\n" \
       "NX-User-Name: %5\r\n" \
       "X-Server-Uuid: %6\r\n" \
       "\r\n"))
            .arg(QString::fromUtf8(H_METHOD)).arg(QString::fromUtf8(H_PATH))
            .arg(url.toString()).arg(QString::fromUtf8(digestHeader.serialized()))
            .arg(admin->getName()).arg(serverUuid.toString())
            .toUtf8();
}

void QnProxySenderConnection::run()
{
    Q_D(QnProxySenderConnection);

    initSystemThreadId();

    auto proxyRequest = makeProxyRequest(d->guid, QUrl(d->proxyServerUrl.address.toString()));
    if (proxyRequest.isEmpty())
    {
        NX_LOG(lit("QnProxySenderConnection: can not generate request")
               .arg(d->proxyServerUrl.toString()), cl_logWARNING);
        return;
    }

    if (!d->socket->connect(d->proxyServerUrl, SOCKET_TIMEOUT))
        return;

    d->socket->setSendTimeout(SOCKET_TIMEOUT);
    d->socket->setRecvTimeout(SOCKET_TIMEOUT);

    int sended = sendRequest(proxyRequest);
    if (sended < proxyRequest.length())
    {
        NX_LOG(lit("QnProxySenderConnection: can not send request to %1")
               .arg(d->proxyServerUrl.toString()), cl_logWARNING);
        d->socket->close();
        return;
    }

    QByteArray response = readProxyResponse();
    if (response.isEmpty())
    {
        NX_LOG(lit("QnProxySenderConnection: no response from %1")
               .arg(d->proxyServerUrl.toString()), cl_logWARNING);
        d->socket->close();
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

    if (!m_needStop && gotRequest)
    {
        parseRequest();
        processRequest();
    }
}
