#include "proxy_connection.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>

#include "api/app_server_connection.h"
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include "core/resource/network_resource.h"
#include "http/custom_headers.h"
#include "media_server/server_message_processor.h"
#include "network/router.h"
#include "network/tcp_listener.h"
#include "network/universal_tcp_listener.h"
#include "proxy_connection_processor_p.h"
#include "transaction/transaction_message_bus.h"

#include <nx/network/socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <utils/common/systemerror.h>

#include "network/universal_tcp_listener.h"
#include "api/app_server_connection.h"
#include "media_server/server_message_processor.h"
#include "core/resource/network_resource.h"
#include "transaction/transaction_message_bus.h"

#include "http/custom_headers.h"
#include "api/global_settings.h"
#include <nx/network/http/auth_tools.h>
#include <nx/network/aio/unified_pollset.h>

#include <utils/common/app_info.h>

class QnTcpListener;

static const int kMaxProxyTtl = 8;
static const std::chrono::milliseconds kIoTimeout = std::chrono::minutes(16);
static const std::chrono::milliseconds kPollTimeout = std::chrono::milliseconds(100);

/** Returns false if socket would block in blocking mode */
static bool readSocketNonBlock(
    int* returnValue, AbstractStreamSocket* socket, 
    void* buffer, int bufferSize)
{
    *returnValue = socket->recv(buffer, bufferSize, MSG_DONTWAIT);
    if (*returnValue < 0)
    {
        auto code = SystemError::getLastOSErrorCode();
        if (code == SystemError::interrupted ||
            code == SystemError::wouldBlock ||
            code == SystemError::again)
        {
            return false;
        }
    }

    return true;
}

// ----------------------------- QnProxyConnectionProcessor ----------------------------

QnProxyConnectionProcessor::QnProxyConnectionProcessor(
    QSharedPointer<AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(new QnProxyConnectionProcessorPrivate, std::move(socket))
{
    Q_D(QnProxyConnectionProcessor);
    d->owner = static_cast<QnUniversalTcpListener*>(owner);
    d->connectTimeout = QnGlobalSettings::instance()->proxyConnectTimeout();
}

QnProxyConnectionProcessor::QnProxyConnectionProcessor(
    QnProxyConnectionProcessorPrivate* priv,
    QSharedPointer<AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(priv, std::move(socket))
{
    Q_D(QnProxyConnectionProcessor);
    d->owner = static_cast<QnUniversalTcpListener*>(owner);
    d->connectTimeout = QnGlobalSettings::instance()->proxyConnectTimeout();
}


QnProxyConnectionProcessor::~QnProxyConnectionProcessor()
{
    pleaseStop();
    stop();
}

bool QnProxyConnectionProcessor::isProtocol(const QString& protocol) const
{
    return protocol == "http" || protocol == "https" || protocol == "rtsp";
}

int QnProxyConnectionProcessor::getDefaultPortByProtocol(const QString& protocol)
{
    QString p = protocol.toLower();
    if (p == "http")
        return 80;
    else if (p == "https")
        return 443;
    else if (p == "rtsp")
        return 554;
    else
        return -1;
}

bool QnProxyConnectionProcessor::doProxyData(AbstractStreamSocket* srcSocket, AbstractStreamSocket* dstSocket, char* buffer, int bufferSize)
{
    int readed;
    if( !readSocketNonBlock(&readed, srcSocket, buffer, bufferSize) )
        return true;

    if (readed < 1)
        return false;
    for( ;; )
    {
        int sended = dstSocket->send(buffer, readed);
        if( sended < 0 )
        {
            if( SystemError::getLastOSErrorCode() == SystemError::interrupted )
                continue;   //retrying interrupted call
            NX_LOG( lit("QnProxyConnectionProcessor::doProxyData. Socket error: %1").arg(SystemError::getLastOSErrorText()), cl_logDEBUG1 );
            return false;
        }
        if( sended == 0 )
            return false;   //socket closed
        if( sended == readed )
            return true;    //sent everything
        buffer += sended;
        readed -= sended;
    }
}

#ifdef PROXY_STRICT_IP
static bool isLocalAddress(const QString& addr)
{
    return addr == lit("localhost") || addr == lit("127.0.0.1");
}
#endif

QString QnProxyConnectionProcessor::connectToRemoteHost(const QnRoute& route, const QUrl& url)
{
    Q_D(QnProxyConnectionProcessor);

    if (route.reverseConnect) {
        const auto& target = route.gatewayId.isNull() ? route.id : route.gatewayId;
        d->dstSocket = d->owner->getProxySocket(
            target.toString(),
            d->connectTimeout.count(),
            [&](int socketCount)
            {
                ec2::QnTransaction<ec2::ApiReverseConnectionData> tran(ec2::ApiCommand::openReverseConnection);
                tran.params.targetServer = qnCommon->moduleGUID();
                tran.params.socketCount = socketCount;
                qnTransactionBus->sendTransaction(tran, target);
            });
    } else {
        d->dstSocket.clear();
    }

    if (!d->dstSocket) {

#ifdef PROXY_STRICT_IP
        QnServerMessageProcessor* processor = dynamic_cast<QnServerMessageProcessor*> (QnServerMessageProcessor::instance());
        if (processor && !processor->isKnownAddr(url.host()) && ! isLocalAddress(url.host()))
            return QString();
#endif

        d->dstSocket = QSharedPointer<AbstractStreamSocket>(
            SocketFactory::createStreamSocket(url.scheme() == lit("https"))
            .release());
        d->dstSocket->setRecvTimeout(d->connectTimeout.count());
        d->dstSocket->setSendTimeout(d->connectTimeout.count());
        if (!d->dstSocket->connect(SocketAddress(url.host().toLatin1().data(), url.port()))) {
            d->socket->close();
            return QString(); // now answer from destination address
        }
        return url.toString();
    }
    else {
        d->dstSocket->setRecvTimeout(d->connectTimeout.count());
        d->dstSocket->setSendTimeout(d->connectTimeout.count());
        return route.id.toString();
    }

    d->dstSocket->setRecvTimeout(kIoTimeout);
    d->dstSocket->setSendTimeout(kIoTimeout);

    return QString();
}


QUrl QnProxyConnectionProcessor::getDefaultProxyUrl()
{
    Q_D(QnProxyConnectionProcessor);
    return QUrl(lit("http://localhost:%1").arg(d->owner->getPort()));
}

/**
* Server nonce could be local since v.3.0. It cause authentication issue while proxing request.
* This function replace user information to the serverAuth key credentials to guarantee
* success authentication on the other server.
*/
bool QnProxyConnectionProcessor::replaceAuthHeader()
{
    Q_D(QnProxyConnectionProcessor);

    QByteArray authHeaderName;
    if (d->request.headers.find(nx_http::header::Authorization::NAME) != d->request.headers.end())
        authHeaderName = nx_http::header::Authorization::NAME;
    else if(d->request.headers.find("Proxy-Authorization") != d->request.headers.end())
        authHeaderName = QByteArray("Proxy-Authorization");
    else
        return true; //< not need to replace anything (proxy without authorization or auth by query items)

    nx_http::header::DigestAuthorization originalAuthHeader;
    if (!originalAuthHeader.parse(nx_http::getHeaderValue(d->request.headers, authHeaderName)))
        return false;
    if (QnUniversalRequestProcessor::needStandardProxy(d->request))
    {
        return true; //< no need to update, it is non server proxy request
    }

    if (auto ownServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID()))
    {
        // it's already authorized request.
        // Update authorization using local system user related to current server
        QString userName = ownServer->getId().toString();
        QString password = ownServer->getAuthKey();

        nx_http::header::WWWAuthenticate wwwAuthenticateHeader;
        nx_http::header::DigestAuthorization digestAuthorizationHeader;
        wwwAuthenticateHeader.authScheme = nx_http::header::AuthScheme::digest;
        wwwAuthenticateHeader.params["nonce"] = QnAuthHelper::instance()->generateNonce(QnAuthHelper::NonceProvider::local);
        wwwAuthenticateHeader.params["realm"] = QnAppInfo::realm().toUtf8();

        if (!nx_http::calcDigestResponse(
            d->request.requestLine.method,
            userName.toUtf8(),
            password.toUtf8(),
            boost::none,
            d->request.requestLine.url.path().toUtf8(),
            wwwAuthenticateHeader,
            &digestAuthorizationHeader))
        {
            return false;
        }

        nx_http::HttpHeader authHeader(authHeaderName, digestAuthorizationHeader.serialized());
        nx_http::HttpHeader userNameHeader(Qn::CUSTOM_USERNAME_HEADER_NAME, originalAuthHeader.digest->userid);
        nx_http::insertOrReplaceHeader(&d->request.headers, authHeader);
        nx_http::insertOrReplaceHeader(&d->request.headers, userNameHeader);
    }

    return true;
}

bool QnProxyConnectionProcessor::updateClientRequest(QUrl& dstUrl, QnRoute& dstRoute)
{
    Q_D(QnProxyConnectionProcessor);

    if (QnUniversalRequestProcessor::needStandardProxy(d->request))
    {
        dstUrl = d->request.requestLine.url;
    }
    else
	{
        QUrl url = d->request.requestLine.url;
        QString host = url.host();
        QString urlPath = QString('/') + QnTcpListener::normalizedPath(url.path());

        // todo: this code is deprecated and isn't compatible with WEB client
        // It never used for WEB client purpose
        if (urlPath.startsWith("proxy") || urlPath.startsWith("/proxy"))
        {
            int proxyEndPos = urlPath.indexOf('/', 2); // remove proxy prefix
            int protocolEndPos = urlPath.indexOf('/', proxyEndPos + 1); // remove proxy prefix
            if (protocolEndPos == -1)
                return false;

            QString protocol = urlPath.mid(proxyEndPos + 1, protocolEndPos - proxyEndPos - 1);
            if (!isProtocol(protocol)) {
                protocol = url.scheme();
                if (protocol.isEmpty())
                    protocol = "http";
                protocolEndPos = proxyEndPos;
            }

            int hostEndPos = urlPath.indexOf('/', protocolEndPos + 1); // remove proxy prefix
            if (hostEndPos == -1)
                hostEndPos = urlPath.size();

            host = urlPath.mid(protocolEndPos + 1, hostEndPos - protocolEndPos - 1);
            if (host.startsWith("{"))
                dstRoute.id = QnUuid::fromStringSafe(host);

            urlPath = urlPath.mid(hostEndPos);

            // get dst ip and port
            QStringList hostAndPort = host.split(':');
            int port = hostAndPort.size() > 1 ? hostAndPort[1].toInt() : getDefaultPortByProtocol(protocol);

            dstUrl = QUrl(lit("%1://%2:%3").arg(protocol).arg(hostAndPort[0]).arg(port));
        }
        else {
            QString scheme = url.scheme();
            if (scheme.isEmpty())
                scheme = lit("http");

            int defaultPort = getDefaultPortByProtocol(scheme);
            dstUrl = QUrl(lit("%1://%2:%3").arg(scheme).arg(url.host()).arg(url.port(defaultPort)));
        }

        if (urlPath.isEmpty())
            urlPath = "/";
        QString query = url.query();
        if (!query.isEmpty()) {
            urlPath += lit("?");
            urlPath += query;
        }
        d->request.requestLine.url = urlPath;
    }

    nx_http::HttpHeaders::const_iterator xCameraGuidIter = d->request.headers.find( Qn::CAMERA_GUID_HEADER_NAME );
    QnUuid cameraGuid;
    if( xCameraGuidIter != d->request.headers.end() )
        cameraGuid = QnUuid::fromStringSafe(xCameraGuidIter->second);
    else
        cameraGuid = QnUuid::fromStringSafe(d->request.getCookieValue(Qn::CAMERA_GUID_HEADER_NAME));
    if (!cameraGuid.isNull()) {
        if (QnResourcePtr camera = qnResPool->getResourceById(cameraGuid))
            dstRoute.id = camera->getParentId();
    }

    nx_http::HttpHeaders::const_iterator itr = d->request.headers.find( Qn::SERVER_GUID_HEADER_NAME );
    if (itr != d->request.headers.end())
        dstRoute.id = QnUuid::fromStringSafe(itr->second);

    if (dstRoute.id == qnCommon->moduleGUID())
    {
        if (!cameraGuid.isNull())
        {
            if (QnNetworkResourcePtr camera = qnResPool->getResourceById<QnNetworkResource>(cameraGuid))
                dstRoute.addr = SocketAddress(camera->getHostAddress(), camera->httpPort());
        }
        else if (QnUniversalRequestProcessor::needStandardProxy(d->request))
        {
            QUrl url = d->request.requestLine.url;
            int defaultPort = getDefaultPortByProtocol(url.scheme());
            dstRoute.addr = SocketAddress(url.host(), url.port(defaultPort));
        }
        else
        {
            //proxying to ourself
            dstRoute.addr = SocketAddress(HostAddress::localhost, d->socket->getLocalAddress().port);
        }
    }
    else if (!dstRoute.id.isNull())
        dstRoute = QnRouter::instance()->routeTo(dstRoute.id);
    else
        dstRoute.addr = SocketAddress(dstUrl.host(), dstUrl.port(80));

    if (!dstRoute.id.isNull() && dstRoute.id != qnCommon->moduleGUID())
    {
        if (!replaceAuthHeader())
            return false;
    }

    if (!dstRoute.addr.isNull())
    {
        if (!dstRoute.gatewayId.isNull())
        {
            nx_http::StringType ttlString = nx_http::getHeaderValue(d->request.headers, Qn::PROXY_TTL_HEADER_NAME);
            bool ok;
            int ttl = ttlString.toInt(&ok);
            if (!ok)
                ttl = kMaxProxyTtl;
            --ttl;

            if (ttl <= 0)
                return false;

            nx_http::insertOrReplaceHeader(&d->request.headers, nx_http::HttpHeader(Qn::PROXY_TTL_HEADER_NAME, QByteArray::number(ttl)));
            nx_http::StringType existAuthSession = nx_http::getHeaderValue(d->request.headers, Qn::AUTH_SESSION_HEADER_NAME);
            if (existAuthSession.isEmpty())
                nx_http::insertOrReplaceHeader(&d->request.headers, nx_http::HttpHeader(Qn::AUTH_SESSION_HEADER_NAME, authSession().toByteArray()));

            QString path = d->request.requestLine.url.path();
            if (!path.startsWith(QLatin1Char('/')))
                path.prepend(QLatin1Char('/'));
            if (dstRoute.id.isNull())
                path.prepend(QString(lit("/proxy/%1/%2:%3")).arg(dstUrl.scheme()).arg(dstUrl.host()).arg(dstUrl.port()));
            else
                path.prepend(QString(lit("/proxy/%1/%2")).arg(dstUrl.scheme()).arg(dstRoute.id.toString()));
            d->request.requestLine.url = path;
        }
        dstUrl.setHost(dstRoute.addr.address.toString());
        dstUrl.setPort(dstRoute.addr.port);

        //adding entry corresponding to current server to Via header
        nx_http::header::Via via;
        auto viaHeaderIter = d->request.headers.find( "Via" );
        if( viaHeaderIter != d->request.headers.end() )
            via.parse( viaHeaderIter->second );

        nx_http::header::Via::ProxyEntry proxyEntry;
        proxyEntry.protoVersion = d->request.requestLine.version.version;
        proxyEntry.receivedBy = qnCommon->moduleGUID().toByteArray();
        via.entries.push_back( proxyEntry );
        nx_http::insertOrReplaceHeader(
            &d->request.headers,
            nx_http::HttpHeader( "Via", via.toString() ) );
    }

    auto hostIter = d->request.headers.find("Host");
    if (hostIter != d->request.headers.end())
        hostIter->second = SocketAddress(
            dstUrl.host(),
            dstUrl.port(nx_http::DEFAULT_HTTP_PORT)).toString().toLatin1();


    //NOTE next hop should accept Authorization header already present
    //  in request since we use current time as nonce value

    d->clientRequest.clear();
    d->request.serialize(&d->clientRequest);

    return true;
}

bool QnProxyConnectionProcessor::openProxyDstConnection()
{
    Q_D(QnProxyConnectionProcessor);

    d->dstSocket.clear();

    // update source request
    QUrl dstUrl;
    QnRoute dstRoute;
    if (!updateClientRequest(dstUrl, dstRoute))
    {
        d->socket->close();
        return false;
    }

    d->lastConnectedUrl = connectToRemoteHost(dstRoute, dstUrl);
    if (d->lastConnectedUrl.isEmpty())
        return false; // invalid dst address

    d->dstSocket->send(d->clientRequest.data(), d->clientRequest.size());

    return true;
}

void QnProxyConnectionProcessor::run()
{
    Q_D(QnProxyConnectionProcessor);

    d->socket->setRecvTimeout(kIoTimeout);
    d->socket->setSendTimeout(kIoTimeout);

    if (d->clientRequest.isEmpty()) {
        d->socket->close();
        return; // no input data
    }

    parseRequest();

    if (!openProxyDstConnection())
        return;

    bool isWebSocket = nx_http::getHeaderValue( d->request.headers, "Upgrade").toLower() == lit("websocket");
    if (!isWebSocket && (d->protocol.toLower() == "http" || d->protocol.toLower() == "https"))
    {
        doSmartProxy();
    }
    else {
        doRawProxy();
    }
    if (d->dstSocket)
        d->dstSocket->close();
    if (d->socket)
        d->socket->close();
}

static const size_t READ_BUFFER_SIZE = 1024*64;

void QnProxyConnectionProcessor::doRawProxy()
{
    Q_D(QnProxyConnectionProcessor);
    nx::Buffer buffer(READ_BUFFER_SIZE, Qt::Uninitialized);

    nx::network::aio::UnifiedPollSet pollSet;
    pollSet.add(d->socket->pollable(), nx::network::aio::etRead);
    pollSet.add(d->dstSocket->pollable(), nx::network::aio::etRead);
    while (!m_needStop)
    {
        int rez = 0;
        auto pollLimitTime = std::chrono::steady_clock::now() + kIoTimeout;
        while (rez == 0)
        {
            if (pollLimitTime <= std::chrono::steady_clock::now())
                return; // IO timeout

            rez = pollSet.poll(kPollTimeout);
            if (rez == -1 && SystemError::getLastOSErrorCode() == SystemError::interrupted)
                rez = 0; // the same as timeout

            if (m_needStop || rez < 0)
                return; // stop or error
        }

        for (auto it = pollSet.begin(); it != pollSet.end(); ++it)
        {
            if (it.eventType() != nx::network::aio::etRead)
            {
                NX_LOGX(lm("Error polling socket: %1").arg(it.socket()), cl_logDEBUG1);
                return;
            }

            if (it.socket() == d->socket->pollable())
            {
                if (!doProxyData(d->socket.data(), d->dstSocket.data(), buffer.data(), buffer.size()))
                    return;
            }
            else if (it.socket() == d->dstSocket->pollable())
            {
                if (!doProxyData(d->dstSocket.data(), d->socket.data(), buffer.data(), buffer.size()))
                    return;
            }
            else
            {
                NX_ASSERT(false, lm("Unexpected pollable: %1").arg(it.socket()));
            }
        }
    }
}

void QnProxyConnectionProcessor::doSmartProxy()
{
    Q_D(QnProxyConnectionProcessor);
    nx::Buffer buffer(READ_BUFFER_SIZE, Qt::Uninitialized);
    d->clientRequest.clear();

    nx::network::aio::UnifiedPollSet pollSet;
    pollSet.add(d->socket->pollable(), nx::network::aio::etRead);
    pollSet.add(d->dstSocket->pollable(), nx::network::aio::etRead);
    while (!m_needStop)
    {
        int rez = 0;
        auto pollLimitTime = std::chrono::steady_clock::now() + kIoTimeout;
        while (rez == 0)
        {
            if (pollLimitTime <= std::chrono::steady_clock::now())
                return; // IO timeout

            rez = pollSet.poll(kPollTimeout);
            if (rez == -1 && SystemError::getLastOSErrorCode() == SystemError::interrupted)
                rez = 0; // the same as timeout

            if (m_needStop || rez < 0)
                return; // stop or error
        }

        for (auto it = pollSet.begin(); it != pollSet.end(); ++it)
        {
            if (it.eventType() != nx::network::aio::etRead)
            {
                NX_LOGX(lm("Error polling socket: %1").arg(it.socket()), cl_logDEBUG1);
                return;
            }

            if (it.socket() == d->socket->pollable())
            {
                int readed;
                if (!readSocketNonBlock(&readed, d->socket.data(), d->tcpReadBuffer, TCP_READ_BUFFER_SIZE))
                    continue;

                if (readed < 1)
                    return;

                d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
                if (isFullMessage(d->clientRequest))
                {
                    parseRequest();
                    QString path = d->request.requestLine.url.path();
                    // parse next request and change dst if required
                    QUrl dstUrl;
                    QnRoute dstRoute;
                    updateClientRequest(dstUrl, dstRoute);
                    bool isWebSocket = nx_http::getHeaderValue( d->request.headers, "Upgrade").toLower() == lit("websocket");
                    bool isSameAddr = d->lastConnectedUrl == dstRoute.addr.toString() || d->lastConnectedUrl == dstUrl;
                    if (isSameAddr)
                    {
                        d->dstSocket->send(d->clientRequest);
                        if (isWebSocket)
                        {
                            if( rez == 2 ) //same as FD_ISSET(d->dstSocket->handle(), &read_set), since we have only 2 sockets
                                if (!doProxyData(d->dstSocket.data(), d->socket.data(), buffer.data(), READ_BUFFER_SIZE))
                                    return; // send rest of data
                            doRawProxy(); // switch to binary mode
                            return;
                        }
                    }
                    else
                    {
                        // new server
                        pollSet.remove(d->dstSocket->pollable(), nx::network::aio::etRead);
                        d->lastConnectedUrl = connectToRemoteHost(dstRoute, dstUrl);
                        if (d->lastConnectedUrl.isEmpty())
                        {
                            d->socket->close();
                            return; // invalid dst address
                        }

                        d->dstSocket->send(d->clientRequest.data(), d->clientRequest.size());

                        if (isWebSocket)
                        {
                            doRawProxy(); // switch to binary mode
                            return;
                        }
                        pollSet.add(d->dstSocket->pollable(), nx::network::aio::etRead);
                        d->clientRequest.clear();
                        break;
                    }
                    d->clientRequest.clear();
                }
            }
            else if (it.socket() == d->dstSocket->pollable())
            {
                if (!doProxyData(d->dstSocket.data(), d->socket.data(), buffer.data(), READ_BUFFER_SIZE))
                    return;
            }
            else
            {
                NX_ASSERT(false, lm("Unexpected pollable: %1").arg(it.socket()));
            }
        }
    }
}
