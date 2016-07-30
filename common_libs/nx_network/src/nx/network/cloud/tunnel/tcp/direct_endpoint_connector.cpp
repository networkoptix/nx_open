/**********************************************************
* Jul 7, 2016
* akolesnikov
***********************************************************/

#include "direct_endpoint_connector.h"

#include <nx/utils/log/log.h>

#include "direct_endpoint_tunnel.h"


namespace nx {
namespace network {
namespace cloud {
namespace tcp {

DirectEndpointConnector::DirectEndpointConnector(
    AddressEntry targetHostAddress,
    nx::String connectSessionId)
:
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(std::move(connectSessionId))
{
}

DirectEndpointConnector::~DirectEndpointConnector()
{
    stopWhileInAioThread();
}

void DirectEndpointConnector::stopWhileInAioThread()
{
    m_connections.clear();
}

int DirectEndpointConnector::getPriority() const
{
    //TODO #ak
    return 0;
}

void DirectEndpointConnector::connect(
    const hpm::api::ConnectResponse& response,
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    using namespace std::placeholders;

    SystemError::ErrorCode sysErrorCode = SystemError::noError;

    NX_ASSERT(!response.forwardedTcpEndpointList.empty());
    for (const SocketAddress& endpoint: response.forwardedTcpEndpointList)
    {
        auto tcpSocket = std::make_unique<TCPSocket>();
        tcpSocket->bindToAioThread(getAioThread());
        if (!tcpSocket->setNonBlockingMode(true) ||
            !tcpSocket->setSendTimeout(timeout.count()))
        {
            sysErrorCode = SystemError::getLastOSErrorCode();
            NX_LOGX(lm("cross-nat %1. Failed to initialize socket for connection to %2: %3")
                .arg(m_connectSessionId).arg(endpoint.toString())
                .arg(SystemError::toString(sysErrorCode)), cl_logDEBUG1);
            continue;
        }
        m_connections.push_back(ConnectionContext{endpoint, std::move(tcpSocket)});
    }

    post(
        [this, sysErrorCode, handler = std::move(handler)]() mutable
        {
            if (m_connections.empty())
            {
                //reporting error
                handler(
                    nx::hpm::api::NatTraversalResultCode::tcpConnectFailed,
                    sysErrorCode,
                    nullptr);
                return;
            }

            m_completionHandler = std::move(handler);
            for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
                it->connection->connectAsync(
                    it->endpoint,
                    std::bind(&DirectEndpointConnector::onConnected, this, _1, it));
        });
}

const AddressEntry& DirectEndpointConnector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

void DirectEndpointConnector::onConnected(
    SystemError::ErrorCode sysErrorCode,
    std::list<ConnectionContext>::iterator socketIter)
{
    auto connectionContext = std::move(*socketIter);
    m_connections.erase(socketIter);

    if (sysErrorCode != SystemError::noError)
    {
        NX_LOGX(lm("cross-nat %1. Tcp connect to %2 has failed: %3")
            .arg(m_connectSessionId).arg(connectionContext.endpoint.toString())
            .arg(SystemError::toString(sysErrorCode)),
            cl_logDEBUG2);
        if (!m_connections.empty())
            return; //waiting for completion of other connections
        auto handler = std::move(m_completionHandler);
        m_completionHandler = nullptr;
        return handler(
            nx::hpm::api::NatTraversalResultCode::tcpConnectFailed,
            sysErrorCode,
            nullptr);
    }

    m_connections.clear();

    auto tunnel =
        std::make_unique<DirectTcpEndpointTunnel>(
            m_connectSessionId,
            std::move(connectionContext.endpoint),
            std::move(connectionContext.connection));
    tunnel->bindToAioThread(getAioThread());

    auto handler = std::move(m_completionHandler);
    m_completionHandler = nullptr;
    handler(
        nx::hpm::api::NatTraversalResultCode::ok,
        SystemError::noError,
        std::move(tunnel));
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
