#include "relay_connector.h"

#include <nx/network/url/url_builder.h>
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
    m_relayClient(
        nx::cloud::relay::api::ClientFactory::create(
            url::Builder().setScheme("http").setEndpoint(relayEndpoint)))
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
    m_relayClient->bindToAioThread(aioThread);
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

    m_relayClient->startSession(
        m_connectSessionId,
        m_targetHostAddress.host.toString().toUtf8(),
        [this](
            nx::cloud::relay::api::ResultCode resultCode,
            nx::cloud::relay::api::CreateClientSessionResponse response)
        {
            onStartRelaySessionResponse(
                resultCode,
                m_relayClient->prevRequestSysErrorCode(),
                std::move(response));
        });
}

const AddressEntry& Connector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

void Connector::stopWhileInAioThread()
{
    m_relayClient.reset();
    m_timer.pleaseStopSync();
}

void Connector::onStartRelaySessionResponse(
    nx::cloud::relay::api::ResultCode resultCode,
    SystemError::ErrorCode sysErrorCode,
    nx::cloud::relay::api::CreateClientSessionResponse response)
{
    decltype(m_handler) handler;
    handler.swap(m_handler);

    if (resultCode != nx::cloud::relay::api::ResultCode::ok)
    {
        m_relayClient.reset();
        m_timer.pleaseStopSync();
        return handler(
            toNatTraversalResultCode(resultCode),
            sysErrorCode,
            nullptr);
    }

    m_connectSessionId = response.sessionId.c_str();

    auto tunnelConnection = std::make_unique<OutgoingTunnelConnection>(
        m_relayEndpoint,
        m_connectSessionId,
        std::move(m_relayClient));
    handler(
        hpm::api::NatTraversalResultCode::ok,
        SystemError::noError,
        std::move(tunnelConnection));
}

void Connector::connectTimedOut()
{
    onStartRelaySessionResponse(
        nx::cloud::relay::api::ResultCode::timedOut,
        SystemError::timedOut,
        nx::cloud::relay::api::CreateClientSessionResponse());
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
