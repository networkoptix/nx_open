#include "relay_connector.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "relay_outgoing_tunnel_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

Connector::Connector(
    SocketAddress relayEndpoint,
    AddressEntry targetHostAddress,
    nx::String connectSessionId)
    :
    m_relayEndpoint(relayEndpoint),
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(std::move(connectSessionId)),
    m_clientToRelayConnection(api::ClientToRelayConnectionFactory::create(relayEndpoint))
{
    bindToAioThread(getAioThread());
}

Connector::~Connector()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void Connector::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_clientToRelayConnection->bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
}

int Connector::getPriority() const
{
    // TODO #ak
    return 0;
}

void Connector::connect(
    const hpm::api::ConnectResponse& /*response*/,
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    m_handler = std::move(handler);

    if (timeout > std::chrono::milliseconds::zero())
        m_timer.start(timeout, std::bind(&Connector::connectTimedOut, this));

    m_clientToRelayConnection->startSession(
        m_connectSessionId,
        m_targetHostAddress.host.toString().toUtf8(),
        [this](
            api::ResultCode resultCode,
            nx::String sessionId)
        {
            onStartRelaySessionResponse(
                resultCode,
                m_clientToRelayConnection->prevRequestSysErrorCode(),
                sessionId);
        });
}

const AddressEntry& Connector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

void Connector::stopWhileInAioThread()
{
    m_clientToRelayConnection.reset();
    m_timer.pleaseStopSync();
}

void Connector::onStartRelaySessionResponse(
    api::ResultCode resultCode,
    SystemError::ErrorCode sysErrorCode,
    nx::String sessionId)
{
    decltype(m_handler) handler;
    handler.swap(m_handler);

    if (resultCode != api::ResultCode::ok)
    {
        m_clientToRelayConnection.reset();
        m_timer.pleaseStopSync();
        return handler(
            toNatTraversalResultCode(resultCode),
            sysErrorCode,
            nullptr);
    }

    m_connectSessionId = sessionId;

    auto tunnelConnection = std::make_unique<OutgoingTunnelConnection>(
        m_relayEndpoint,
        m_connectSessionId,
        std::move(m_clientToRelayConnection));
    handler(
        hpm::api::NatTraversalResultCode::ok,
        SystemError::noError,
        std::move(tunnelConnection));
}

void Connector::connectTimedOut()
{
    onStartRelaySessionResponse(
        api::ResultCode::timedOut,
        SystemError::timedOut,
        nx::String());
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
