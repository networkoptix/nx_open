
#include "universal_tcp_listener.h"

#include <common/common_module.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/retry_timer.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/async_client.h>
#include <nx/utils/log/log.h>

#include "cloud/cloud_connection_manager.h"
#include "proxy_sender_connection_processor.h"
#include "universal_request_processor.h"


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

    auto regularSocket =
        QnHttpConnectionListener::createAndPrepareSocket(sslNeeded, localAddress);
    if (!regularSocket)
        return nullptr;

    auto multipleServerSocket = std::make_unique<nx::network::MultipleServerSocket>();
    if (!multipleServerSocket->addSocket(
            std::unique_ptr<AbstractStreamServerSocket>(regularSocket)))
        return nullptr;
    m_multipleServerSocket = std::move(multipleServerSocket);

    if (m_boundToCloud)
        updateCloudConnectState(&lk);

    return m_multipleServerSocket.get();
}

void QnUniversalTcpListener::destroyServerSocket(
    AbstractStreamServerSocket* serverSocket)
{
    QnMutexLocker lk(&m_mutex);

    NX_ASSERT(m_multipleServerSocket.get() == serverSocket);
    m_multipleServerSocket->pleaseStopSync();
    m_multipleServerSocket.reset();
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
        NX_ASSERT(m_multipleServerSocket->count() == 1);

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
        NX_ASSERT(m_multipleServerSocket->count() == 2);
        m_multipleServerSocket->removeSocket(1);
    }
}
