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
    nx::utils::Url relayUrl,
    AddressEntry targetHostAddress,
    nx::String connectSessionId)
    :
    m_relayUrl(std::move(relayUrl)),
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(std::move(connectSessionId)),
    m_relayClient(
        nx::cloud::relay::api::ClientFactory::create(m_relayUrl))
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

    dispatch(
        [this, timeout]()
        {
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

            if (timeout > std::chrono::milliseconds::zero())
                m_timer.start(timeout, std::bind(&Connector::connectTimedOut, this));
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

    m_timer.pleaseStopSync();

    if (resultCode != nx::cloud::relay::api::ResultCode::ok)
    {
        m_relayClient.reset();
        return handler(
            toNatTraversalResultCode(resultCode),
            sysErrorCode,
            nullptr);
    }

    m_connectSessionId = response.sessionId.c_str();

    auto tunnelConnection = std::make_unique<OutgoingTunnelConnection>(
        nx::utils::Url(response.actualRelayUrl.c_str()),
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
