#include "proxy_sender_connection_processor.h"

#include <QtCore/QElapsedTimer>

#include "common/common_globals.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/user_resource.h"
#include "network/tcp_connection_priv.h"
#include "network/tcp_listener.h"
#include "network/universal_request_processor_p.h"
#include "network/universal_request_processor_p.h"
#include "network/tcp_connection_priv.h"
#include "network/tcp_listener.h"
#include <nx/utils/log/log.h>
#include "utils/common/synctime.h"
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/auth_tools.h>

#include "universal_tcp_listener.h"
#include <core/resource/media_server_resource.h>
#include <nx/network/http/custom_headers.h>

#include <utils/common/app_info.h>

static const int kSocketTimeout = 1000 * 5;
static const int kProxyKeepAliveInterval = 60 * 1000;

class QnProxySenderConnectionPrivate: public QnUniversalRequestProcessorPrivate
{
public:
    SocketAddress proxyServerUrl;
    QnUuid guid;
};

QnProxySenderConnection::QnProxySenderConnection(
    const SocketAddress& proxyServerUrl,
    const QnUuid& guid,
    QnUniversalTcpListener* owner,
    bool needAuth)
    :
    QnUniversalRequestProcessor(
        new QnProxySenderConnectionPrivate,
        QSharedPointer<AbstractStreamSocket>(SocketFactory::createStreamSocket().release()),
        owner,
        needAuth)
{
    Q_D(QnProxySenderConnection);
    d->proxyServerUrl = proxyServerUrl;
    d->guid = guid;
    setObjectName(toString(this));
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
        int bytesRead = d->socket->recv(
            buffer + bufLen, sizeof(buffer) - static_cast<unsigned int>(bufLen));
        if (bytesRead < 1)
            return QByteArray();
        bufLen += bytesRead;
        QByteArray result = QByteArray::fromRawData((const char*) buffer, (int) bufLen);

        const auto messageSize = QnTCPConnectionProcessor::isFullMessage(result);
        if (messageSize < 0)
            return QByteArray();

        if (messageSize > 0)
            return result;
    }

    return QByteArray();
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

QByteArray QnProxySenderConnection::makeProxyRequest(
    const QnUuid& serverUuid, const SocketAddress& address) const
{
    const QByteArray H_REALM("NX");
    const QByteArray H_METHOD("CONNECT");
    const QByteArray H_PATH("/proxy-reverse");
    const QByteArray H_AUTH("auth-int");

    QnMediaServerResourcePtr server = resourcePool()->getResourceById<QnMediaServerResource>(
        serverUuid);
    if (!server)
        return QByteArray();

    const auto time = qnSyncTime->currentUSecsSinceEpoch();

    nx_http::header::WWWAuthenticate authHeader;
    authHeader.authScheme = nx_http::header::AuthScheme::digest;
    authHeader.params["nonce"] = QString::number(time, 16).toLatin1();
    authHeader.params["realm"] = nx::network::AppInfo::realm().toLatin1();

    nx_http::header::DigestAuthorization digestHeader;
    if (!nx_http::calcDigestResponse(
        H_METHOD,
        server->getId().toByteArray(),
        server->getAuthKey().toUtf8(),
        boost::none,
        H_PATH,
        authHeader,
        &digestHeader))
    {
        return QByteArray();
    }

    static const auto kRequest = lm(
        "%1 %2 HTTP/1.1\r\n"
        "Host: %3\r\n"
        "Authorization: %4\r\n"
        "%5: %6\r\n"
        "\r\n");

    return kRequest.args(H_METHOD, H_PATH, address, digestHeader.serialized(),
        Qn::PROXY_SENDER_HEADER_NAME, serverUuid.toString()).toUtf8();
}

void QnProxySenderConnection::run()
{
    Q_D(QnProxySenderConnection);

    initSystemThreadId();

    auto proxyRequest = makeProxyRequest(d->guid, d->proxyServerUrl);
    if (proxyRequest.isEmpty())
    {
        NX_WARNING(this, lm("Can not generate request").arg(d->proxyServerUrl));
        return;
    }

    if (!d->socket->connect(d->proxyServerUrl, kSocketTimeout))
        return;

    d->socket->setSendTimeout(kSocketTimeout);
    d->socket->setRecvTimeout(kSocketTimeout);

    int bytesSent = sendRequest(proxyRequest);
    if (bytesSent < proxyRequest.length())
    {
        NX_WARNING(this, lm("Can not send request to %1").arg(d->proxyServerUrl));
        d->socket->close();
        return;
    }

    QByteArray response = readProxyResponse();
    if (response.isEmpty())
    {
        NX_WARNING(this, lm("No response from %1").arg(d->proxyServerUrl));
        d->socket->close();
        return;
    }

    // Wait for the main request from the remote host.
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
                gotRequest = false; //< proxy keep-alive packets
            else
                break;
        }
        else
        {
            if (timer.elapsed() > kProxyKeepAliveInterval)
                break;
        }
    }

    if (!gotRequest && timer.elapsed() < kProxyKeepAliveInterval)
        doDelay();

    if (!m_needStop && gotRequest)
    {
        parseRequest();
        NX_VERBOSE(this, lm("Process request %1").arg(d->request.requestLine));

        auto owner = static_cast<QnUniversalTcpListener*>(d->owner);
        auto handler = owner->findHandler(d->protocol, d->request);
        bool noAuth = false;
        if (handler && authenticate(&d->accessRights, &noAuth))
            processRequest(noAuth);
    }
}
