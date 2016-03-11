
#include "universal_tcp_listener.h"

#include <common/common_module.h>
#include <nx/utils/log/log.h>

#include "universal_request_processor.h"
#include "proxy_sender_connection_processor.h"


QnUniversalTcpListener::QnUniversalTcpListener(
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
    m_boundToCloud(false)
{
    m_cloudCredentials.serverId = ...;
}

void QnUniversalTcpListener::addProxySenderConnections(const SocketAddress& proxyUrl, int size)
{
    if (m_needStop)
        return;

    NX_LOG(lit("QnHttpConnectionListener: %1 reverse connection(s) to %2 is(are) needed")
        .arg(size).arg(proxyUrl.toString()), cl_logDEBUG1);

    for (int i = 0; i < size; ++i)
    {
        auto connect = new QnProxySenderConnection(proxyUrl, qnCommon->moduleGUID(), this);
        connect->start();
        addOwnership(connect);
    }
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket)
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

    updateCloudConnectState(&lk);

    return m_multipleServerSocket.get();
}

void QnUniversalTcpListener::destroyServerSocket(
    AbstractStreamServerSocket* serverSocket)
{
    QnMutexLocker lk(&m_mutex);

    NX_ASSERT(m_multipleServerSocket.get() == serverSocket);
    m_multipleServerSocket.reset();
}

void QnUniversalTcpListener::onCloudBindingStatusChanged(
    bool isBound,
    const QString& cloudSystemId,
    const QString& cloudAuthenticationKey)
{
    QnMutexLocker lk(&m_mutex);

    const bool newCredentialsAreTheSame = 
        m_cloudCredentials.systemId == cloudSystemId.toUtf8() &&
        m_cloudCredentials.key == cloudAuthenticationKey.toUtf8();

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
    m_cloudCredentials.systemId = cloudSystemId.toUtf8();
    m_cloudCredentials.key = cloudAuthenticationKey.toUtf8();
    updateCloudConnectState(&lk);
}

void QnUniversalTcpListener::updateCloudConnectState(
    QnMutexLockerBase* const lk)
{
    if (!m_multipleServerSocket)
        return;

    if (m_boundToCloud)
    {
        NX_ASSERT(m_multipleServerSocket->size() == 1);

        auto cloudServerSocket = std::make_unique<CloudServerSocket>();
        cloudServerSocket->setCloudCredentials(m_cloudCredentials);
        //TODO #ak should not fail on first error, but try to restore...
            //E.g., system binding data can be propagated to mediator with some delay
        cloudServerSocket->listen();
        m_multipleServerSocket->addSocket(std::move(cloudServerSocket));
    }
    else
    {
        if (m_multipleServerSocket->size() == 2)
            m_multipleServerSocket->removeSocket(1);
    }
}
