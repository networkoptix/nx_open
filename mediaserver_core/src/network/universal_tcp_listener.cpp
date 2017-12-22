#include "universal_tcp_listener.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/retry_timer.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/log/log.h>

#include <nx/vms/cloud_integration/cloud_connection_manager.h>

#include <common/common_module.h>

#include "proxy_sender_connection_processor.h"
#include "universal_request_processor.h"

QnUniversalTcpListener::QnUniversalTcpListener(
    QnCommonModule* commonModule,
    const nx::vms::cloud_integration::CloudConnectionManager& cloudConnectionManager,
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSsl)
:
    QnHttpConnectionListener(
        commonModule,
        address,
        port,
        maxConnections,
        useSsl),
    m_cloudConnectionManager(cloudConnectionManager),
    m_boundToCloud(false),
    m_httpModManager(new nx_http::HttpModManager())
{
    m_cloudCredentials.serverId = commonModule->moduleGUID().toByteArray();
    Qn::directConnect(
        &cloudConnectionManager, &nx::vms::cloud_integration::CloudConnectionManager::cloudBindingStatusChanged,
        this,
        [this, &cloudConnectionManager](bool /*boundToCloud*/)
        {
            onCloudBindingStatusChanged(cloudConnectionManager.getSystemCredentials());
        });
    onCloudBindingStatusChanged(cloudConnectionManager.getSystemCredentials());
}

QnUniversalTcpListener::~QnUniversalTcpListener()
{
    stop();
    directDisconnectAll();
}

void QnUniversalTcpListener::addProxySenderConnections(
    const SocketAddress& proxyUrl,
    int size)
{
    if (m_needStop)
        return;

    NX_LOG(lit("QnHttpConnectionListener: %1 reverse connection(s) to %2 is(are) needed")
        .arg(size).arg(proxyUrl.toString()), cl_logDEBUG1);

    for (int i = 0; i < size; ++i)
    {
        auto connect = new QnProxySenderConnection(
            proxyUrl, commonModule()->moduleGUID(), this, needAuth());

        connect->start();
        addOwnership(connect);
    }
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(
    QSharedPointer<AbstractStreamSocket> clientSocket)
{
    return new QnUniversalRequestProcessor(clientSocket, this, needAuth());
}

AbstractStreamServerSocket* QnUniversalTcpListener::addServerSocketToMultipleSocket(
    const SocketAddress& localAddress,
    nx::network::MultipleServerSocket* multipleServerSocket,
    int ipVersion)
{
    std::unique_ptr<AbstractStreamServerSocket> tcpServerSocket;
    if (m_preparedTcpSocket
        && m_preparedTcpSocket->getLocalAddress().toString() == localAddress.toString())
    {
        tcpServerSocket = std::move(m_preparedTcpSocket);
    }

    if (!tcpServerSocket)
        tcpServerSocket = createAndPrepareTcpSocket(localAddress);

    if (!tcpServerSocket)
    {
        setLastError(SystemError::getLastOSErrorCode());
        return nullptr;
    }

    auto result = tcpServerSocket.get();
    if (!multipleServerSocket->addSocket(std::move(tcpServerSocket)))
    {
        setLastError(SystemError::getLastOSErrorCode());
        return nullptr;
    }

    return result;
}

AbstractStreamServerSocket* QnUniversalTcpListener::createAndPrepareSocket(
    bool sslNeeded,
    const SocketAddress& localAddress)
{
    QnMutexLocker lk(&m_mutex);

    auto multipleServerSocket = std::make_unique<nx::network::MultipleServerSocket>();
    bool needToAddIpV4Socket =
        localAddress.address.toString() == HostAddress::anyHost.toString()
        || (bool) localAddress.address.ipV4();

    bool needToAddIpV6Socket =
        localAddress.address.toString() == HostAddress::anyHost.toString()
        || (bool)localAddress.address.isPureIpV6();

    AbstractStreamServerSocket* ipV4ServerSocket = nullptr;
    if (needToAddIpV4Socket)
    {
        ipV4ServerSocket = addServerSocketToMultipleSocket(
            localAddress,
            multipleServerSocket.get(),
            AF_INET);

        if (ipV4ServerSocket == nullptr)
            return nullptr;
        ++m_totalListeningSockets;
        ++m_cloudSocketIndex;
    }

    if (needToAddIpV6Socket)
    {
        SocketAddress ipV6SocketAddress = localAddress;
        if (ipV4ServerSocket != nullptr)
            ipV6SocketAddress.port = ipV4ServerSocket->getLocalAddress().port;

        if (!addServerSocketToMultipleSocket(
                ipV6SocketAddress,
                multipleServerSocket.get(),
                AF_INET6))
        {
            return nullptr;
        }
        ++m_totalListeningSockets;
        ++m_cloudSocketIndex;
    }

    #ifdef LISTEN_ON_UDT_SOCKET
        auto udtServerSocket = std::make_unique<nx::network::UdtStreamServerSocket>();
        if (!udtServerSocket->setReuseAddrFlag(true) ||
            !udtServerSocket->bind(localAddress) ||
            !udtServerSocket->listen() ||
            !multipleServerSocket->addSocket(std::move(udtServerSocket)))
        {
            setLastError(SystemError::getLastOSErrorCode());
            return nullptr;
        }
    #endif

    m_multipleServerSocket = multipleServerSocket.get();
    m_serverSocket = std::move(multipleServerSocket);

    if (m_boundToCloud)
        updateCloudConnectState(&lk);

    #ifdef ENABLE_SSL
        if (sslNeeded)
        {
            m_serverSocket = std::make_unique<nx::network::deprecated::SslServerSocket>(
                std::move(m_serverSocket),
                true);
        }
    #endif

   return m_serverSocket.get();
}

void QnUniversalTcpListener::destroyServerSocket(
    AbstractStreamServerSocket* serverSocket)
{
    decltype(m_serverSocket) serverSocketToDestroy;
    {
        QnMutexLocker lk(&m_mutex);

        NX_ASSERT(m_serverSocket.get() == serverSocket);
        std::swap(m_serverSocket, serverSocketToDestroy);
    }

    serverSocketToDestroy->pleaseStopSync();
    serverSocketToDestroy.reset();
}

void QnUniversalTcpListener::onCloudBindingStatusChanged(
    boost::optional<nx::hpm::api::SystemCredentials> cloudCredentials)
{
    QnMutexLocker lk(&m_mutex);

    const bool isBound = static_cast<bool>(cloudCredentials);
    const bool newCredentialsAreTheSame =
        cloudCredentials
        ? (m_cloudCredentials == *cloudCredentials)
        : false;

    if (m_boundToCloud == isBound &&
        (!m_boundToCloud || newCredentialsAreTheSame))
    {
        //no need to do anything
        return;
    }

    if ((m_boundToCloud == isBound) && m_boundToCloud && !newCredentialsAreTheSame)
    {
        //unbinding from cloud first
        m_boundToCloud = false;
        updateCloudConnectState(&lk);
    }

    m_boundToCloud = isBound;
    if (cloudCredentials)
        m_cloudCredentials = *cloudCredentials;
    updateCloudConnectState(&lk);
}

void QnUniversalTcpListener::updateCloudConnectState(
    QnMutexLockerBase* const /*lk*/)
{
    if (!m_multipleServerSocket)
        return;

    NX_LOGX(lm("Update cloud connect state (boundToCloud=%1)").arg(m_boundToCloud), cl_logINFO);
    if (m_boundToCloud)
    {
        NX_ASSERT(m_multipleServerSocket->count() == m_cloudSocketIndex);

        nx::network::RetryPolicy registrationOnMediatorRetryPolicy;
        registrationOnMediatorRetryPolicy.maxRetryCount =
            nx::network::RetryPolicy::kInfiniteRetries;

        auto cloudServerSocket =
            std::make_unique<nx::network::cloud::CloudServerSocket>(
                &nx::network::SocketGlobals::cloud().mediatorConnector(),
                std::move(registrationOnMediatorRetryPolicy));
        cloudServerSocket->listen(0);
        m_multipleServerSocket->addSocket(std::move(cloudServerSocket));
    }
    else
    {
        NX_ASSERT(m_multipleServerSocket->count() == m_totalListeningSockets);
        m_multipleServerSocket->removeSocket(m_cloudSocketIndex);
    }
}

void QnUniversalTcpListener::applyModToRequest(nx_http::Request* request)
{
    m_httpModManager->apply(request);
}

bool QnUniversalTcpListener::isAuthentificationRequired(nx_http::Request& request)
{
    const auto targetHeader = request.headers.find(Qn::SERVER_GUID_HEADER_NAME);
    if (targetHeader == request.headers.end())
        return true; //< Only server proxy allowed.

    if (QnUuid(targetHeader->second) == commonModule()->moduleGUID())
        return true; //< Self proxy is not a proxy.

    if (!QUrlQuery(request.requestLine.url.toQUrl()).hasQueryItem(lit("authKey")))
        return true; //< Temporary authorization is required.

    const auto requestPath = request.requestLine.url.path();
    for (const auto& path: m_unauthorizedForwardingPaths)
    {
        if (requestPath.startsWith(path))
            return false;
    }

    return true;
}

void QnUniversalTcpListener::enableUnauthorizedForwarding(const QString& path)
{
    m_unauthorizedForwardingPaths.insert(lit("/") + path + lit("/"));
}

void QnUniversalTcpListener::setPreparedTcpSocket(std::unique_ptr<AbstractStreamServerSocket> socket)
{
    m_preparedTcpSocket = std::move(socket);
}

std::unique_ptr<AbstractStreamServerSocket> QnUniversalTcpListener::createAndPrepareTcpSocket(
    const SocketAddress& localAddress)
{
    auto socket = SocketFactory::createStreamServerSocket(
        false,
        nx::network::NatTraversalSupport::enabled
        // TODO: #muskov: Fix.
        /*, ipVersion*/);

    if (!socket->setReuseAddrFlag(true) ||
        !socket->bind(localAddress) ||
        !socket->listen())
    {
        return nullptr;
    }

    return socket;
}

nx_http::HttpModManager* QnUniversalTcpListener::httpModManager() const
{
    return m_httpModManager.get();
}
