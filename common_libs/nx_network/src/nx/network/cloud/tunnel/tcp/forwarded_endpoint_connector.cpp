/**********************************************************
* Jul 7, 2016
* akolesnikov
***********************************************************/

#include "forwarded_endpoint_connector.h"

#include <nx/utils/log/log.h>

#include "forwarded_endpoint_tunnel.h"


namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ForwardedEndpointConnector::ForwardedEndpointConnector(
    AddressEntry targetHostAddress,
    nx::String connectSessionId)
:
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(std::move(connectSessionId))
{
}

ForwardedEndpointConnector::~ForwardedEndpointConnector()
{
    stopWhileInAioThread();
}

void ForwardedEndpointConnector::stopWhileInAioThread()
{
    m_connections.clear();
}

int ForwardedEndpointConnector::getPriority() const
{
    //TODO #ak
    return 0;
}

void ForwardedEndpointConnector::connect(
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
                    std::bind(&ForwardedEndpointConnector::onConnected, this, _1, it));
        });
}

const AddressEntry& ForwardedEndpointConnector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

void ForwardedEndpointConnector::onConnected(
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
        std::make_unique<ForwardedTcpEndpointTunnel>(
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
