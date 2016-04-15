
#include "universal_tcp_listener.h"

#include <common/common_module.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/retry_timer.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/system_socket.h>
#include <nx/utils/log/log.h>

#include "cloud/cloud_connection_manager.h"
#include "proxy_sender_connection_processor.h"
#include "universal_request_processor.h"


//#define LISTEN_ON_UDT_SOCKET
#ifdef LISTEN_ON_UDT_SOCKET
static const int kCloudSocketIndex = 2;
static const int kTotalListeningSockets = 3;
#else
static const int kCloudSocketIndex = 1;
static const int kTotalListeningSockets = 2;
#endif

QnUniversalTcpListener::QnUniversalTcpListener(
    const CloudConnectionManager& cloudConnectionManager,
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSsl)
:
    QnHttpConnectionListener(
        address,
        port,
        maxConnections,
        useSsl),
    m_cloudConnectionManager(cloudConnectionManager),
    m_boundToCloud(false)
{
    m_cloudCredentials.serverId = qnCommon->moduleGUID().toByteArray();
    Qn::directConnect(
        &cloudConnectionManager, &CloudConnectionManager::cloudBindingStatusChanged,
        this,
        [this, &cloudConnectionManager](bool /*bindedToCloud*/)
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
            proxyUrl, qnCommon->moduleGUID(), this);
        connect->start();
        addOwnership(connect);
    }
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(
    QSharedPointer<AbstractStreamSocket> clientSocket)
{
    return new QnUniversalRequestProcessor(clientSocket, this, needAuth());
}

AbstractStreamServerSocket* QnUniversalTcpListener::createAndPrepareSocket(
    bool sslNeeded,
    const SocketAddress& localAddress)
{
    QnMutexLocker lk(&m_mutex);

    auto tcpServerSocket = std::make_unique<nx::network::TCPServerSocket>();
    if (!tcpServerSocket->setReuseAddrFlag(true) ||
        !tcpServerSocket->bind(localAddress) ||
        !tcpServerSocket->listen())
    {
        return nullptr;
    }

    //auto sslServerSocket = std::make_unique<nx::network::SslServerSocket>(
    //    tcpServerSocket.release(), true );

    auto multipleServerSocket = std::make_unique<nx::network::MultipleServerSocket>();
    if (!multipleServerSocket->addSocket(std::move(tcpServerSocket)))
        return nullptr;

#ifdef LISTEN_ON_UDT_SOCKET
    auto udtServerSocket = std::make_unique<nx::network::UdtStreamServerSocket>();
    if (!udtServerSocket->setReuseAddrFlag(true) ||
        !udtServerSocket->bind(localAddress) ||
        !udtServerSocket->listen())
    {
        return nullptr;
    }
    multipleServerSocket->addSocket(std::move(udtServerSocket));
#endif

    m_multipleServerSocket = multipleServerSocket.get();
    m_serverSocket = std::move(multipleServerSocket);

    if (m_boundToCloud)
        updateCloudConnectState(&lk);


    #ifdef ENABLE_SSL
        m_serverSocket.reset( new nx::network::SslServerSocket( m_serverSocket.release(), true ) );
    #endif


   return m_serverSocket.get();
}

void QnUniversalTcpListener::destroyServerSocket(
    AbstractStreamServerSocket* serverSocket)
{
    QnMutexLocker lk(&m_mutex);

    NX_ASSERT(m_serverSocket.get() == serverSocket);
    m_serverSocket->pleaseStopSync();
    m_serverSocket.reset();
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

    if (m_boundToCloud)
    {
        NX_ASSERT(m_multipleServerSocket->count() == kCloudSocketIndex);

        nx::network::RetryPolicy registrationOnMediatorRetryPolicy;
        registrationOnMediatorRetryPolicy.setMaxRetryCount(
            nx::network::RetryPolicy::kInfiniteRetries);

        auto cloudServerSocket =
            std::make_unique<nx::network::cloud::CloudServerSocket>(
                nx::network::SocketGlobals::mediatorConnector().systemConnection(),
                std::move(registrationOnMediatorRetryPolicy));
        cloudServerSocket->listen(0);
        m_multipleServerSocket->addSocket(std::move(cloudServerSocket));
    }
    else
    {
        NX_ASSERT(m_multipleServerSocket->count() == kTotalListeningSockets);
        m_multipleServerSocket->removeSocket(kCloudSocketIndex);
    }
}
