#include "universal_tcp_listener.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/retry_timer.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/log/log.h>
#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <nx/vms/cloud_integration/cloud_manager_group.h>

#include <common/common_module.h>

#include "universal_request_processor.h"

QnUniversalTcpListener::QnUniversalTcpListener(
    QnCommonModule* commonModule,
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
    m_boundToCloud(false),
    m_httpModManager(new nx::network::http::HttpModManager())
{
    m_cloudCredentials.serverId = commonModule->moduleGUID().toByteArray();
}

void QnUniversalTcpListener::setupAuthorizer(
    TimeBasedNonceProvider* timeBasedNonceProvider,
    nx::vms::cloud_integration::CloudManagerGroup& cloudManagerGroup)
{
    m_authenticator = std::make_unique<nx::vms::server::Authenticator>(
        commonModule(), timeBasedNonceProvider,
        &cloudManagerGroup.authenticationNonceFetcher,
        &cloudManagerGroup.userAuthenticator);
}

void QnUniversalTcpListener::setCloudConnectionManager(
    const nx::vms::cloud_integration::CloudConnectionManager& cloudConnectionManager)
{
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
}

void QnUniversalTcpListener::stop()
{
    directDisconnectAll();
    QnHttpConnectionListener::stop();
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket)
{
    return new QnUniversalRequestProcessor(std::move(clientSocket), this, needAuth());
}

nx::network::AbstractStreamServerSocket* QnUniversalTcpListener::createAndPrepareSocket(
    bool sslNeeded,
    const nx::network::SocketAddress& localAddress)
{
    QnMutexLocker lk(&m_mutex);
    auto tcpSockets = createAndPrepareTcpSockets(localAddress);

    if (tcpSockets.empty())
        return nullptr;

    auto multipleServerSocket = std::make_unique<nx::network::MultipleServerSocket>();
    for (auto& socket: tcpSockets)
    {
        if (!multipleServerSocket->addSocket(std::move(socket)))
        {
            setLastError(SystemError::getLastOSErrorCode());
            return nullptr;
        }
    }

    m_multipleServerSocket = multipleServerSocket.get();
    m_serverSocket = std::move(multipleServerSocket);

    if (m_boundToCloud)
        updateCloudConnectState(&lk);

    #ifdef ENABLE_SSL
        if (sslNeeded)
        {
            m_serverSocket = nx::network::SocketFactory::createSslAdapter(
                std::exchange(m_serverSocket, nullptr),
                nx::network::ssl::EncryptionUse::autoDetectByReceivedData);
        }
    #endif

   return m_serverSocket.get();
}

void QnUniversalTcpListener::destroyServerSocket(
    nx::network::AbstractStreamServerSocket* serverSocket)
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
    std::optional<nx::hpm::api::SystemCredentials> cloudCredentials)
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

    NX_INFO(this, lm("Update cloud connect state (boundToCloud=%1)").arg(m_boundToCloud));
    if (m_boundToCloud)
    {
        nx::network::RetryPolicy registrationOnMediatorRetryPolicy;
        registrationOnMediatorRetryPolicy.maxRetryCount =
            nx::network::RetryPolicy::kInfiniteRetries;

        auto cloudServerSocket =
            std::make_unique<nx::network::cloud::CloudServerSocket>(
                &nx::network::SocketGlobals::cloud().mediatorConnector(),
                std::move(registrationOnMediatorRetryPolicy));
        cloudServerSocket->listen(0);
        m_multipleServerSocket->addSocket(std::move(cloudServerSocket));
        m_cloudSocketIndex = (int) m_multipleServerSocket->count() - 1;
    }
    else
    {
        m_multipleServerSocket->removeSocket(m_cloudSocketIndex);
    }
}

void QnUniversalTcpListener::applyModToRequest(nx::network::http::Request* request)
{
    m_httpModManager->apply(request);
}

nx::vms::server::Authenticator* QnUniversalTcpListener::authenticator() const
{
    return m_authenticator.get();
}

nx::vms::server::Authenticator* QnUniversalTcpListener::authenticator(const QnTcpListener* listener)
{
    const auto universalListener = dynamic_cast<const QnUniversalTcpListener*>(listener);
    NX_CRITICAL(universalListener);
    return universalListener->authenticator();
}

bool QnUniversalTcpListener::isAuthentificationRequired(nx::network::http::Request& request)
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


std::vector<std::unique_ptr<nx::network::AbstractStreamServerSocket>>
    QnUniversalTcpListener::createAndPrepareTcpSockets(const nx::network::SocketAddress& localAddress)
{
    uint16_t assignedPort = 0;
    std::vector<std::unique_ptr<nx::network::AbstractStreamServerSocket>> sockets;
    const auto addSocket =
        [&](nx::network::SocketAddress localAddress, int ipVersion)
        {
            if (assignedPort)
                localAddress.port = assignedPort;

            auto socket = nx::network::SocketFactory::createStreamServerSocket(
                false,
                nx::network::NatTraversalSupport::enabled,
                ipVersion);

            if (!socket->setReuseAddrFlag(true) ||
                !socket->setReusePortFlag(true) ||
                !socket->bind(localAddress) ||
                !socket->listen())
            {
                NX_WARNING(
                    typeid(QnUniversalTcpListener),
                    lm("Failed to create server socket for address: %1, family: %2")
                        .args(localAddress, (ipVersion == AF_INET ? "IpV4" : "IpV6")));
                return false;
            }

            assignedPort = socket->getLocalAddress().port;
            sockets.push_back(std::move(socket));
            return true;
        };

    const bool isAnyHost = localAddress.address.toString() == nx::network::HostAddress::anyHost.toString();
    if (isAnyHost || (bool) localAddress.address.ipV4())
        addSocket(localAddress, AF_INET);

    if (isAnyHost || (bool) localAddress.address.isPureIpV6())
        addSocket(localAddress, AF_INET6);

    return sockets;
}

nx::network::http::HttpModManager* QnUniversalTcpListener::httpModManager() const
{
    return m_httpModManager.get();
}
