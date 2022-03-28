// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_connector.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "relay_outgoing_tunnel_connection.h"

namespace nx::network::cloud::relay {

Connector::Connector(
    nx::utils::Url relayUrl,
    AddressEntry targetHostAddress,
    std::string connectSessionId)
    :
    m_relayUrl(std::move(relayUrl)),
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(std::move(connectSessionId)),
    m_relayClient(
        std::make_unique<nx::cloud::relay::api::Client>(m_relayUrl))
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
    // TODO #akolesnikov
    return 0;
}

void Connector::connect(
    const hpm::api::ConnectResponse& /*response*/,
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    NX_VERBOSE(this, "%1. Starting relay connection to %2", m_connectSessionId, m_targetHostAddress);

    m_handler = std::move(handler);

    dispatch(
        [this, timeout]()
        {
            m_relayClient->setTimeout(timeout);

            m_relayClient->startSession(
                m_connectSessionId,
                m_targetHostAddress.host.toString(),
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
    NX_VERBOSE(this, "%1. Received relay start session response %2, %3",
        m_connectSessionId, resultCode, SystemError::toString(sysErrorCode));

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

    if (response.sessionId != m_connectSessionId)
    {
        NX_VERBOSE(this, "%1. Switching to relay session id %2",
            m_connectSessionId, response.sessionId);
        m_connectSessionId = response.sessionId;
    }

    auto tunnelConnection = std::make_unique<OutgoingTunnelConnection>(
        nx::utils::Url(response.actualRelayUrl),
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

} // namespace nx::network::cloud::relay
