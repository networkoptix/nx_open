// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_connector.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "nx/network/cloud/data/connection_result_data.h"
#include "nx/network/http/http_status.h"
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
    m_timeoutTimer.bindToAioThread(aioThread);
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
            m_responseTimer.restart();
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
                        m_relayClient->prevRequestHttpStatusCode(),
                        std::move(response));
                });

            if (timeout > std::chrono::milliseconds::zero())
                m_timeoutTimer.start(timeout, std::bind(&Connector::connectTimedOut, this));
        });
}

const AddressEntry& Connector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

void Connector::stopWhileInAioThread()
{
    m_relayClient.reset();
    m_timeoutTimer.pleaseStopSync();
}

void Connector::onStartRelaySessionResponse(
    nx::cloud::relay::api::ResultCode resultCode,
    SystemError::ErrorCode sysErrorCode,
    nx::network::http::StatusCode::Value httpStatusCode,
    nx::cloud::relay::api::CreateClientSessionResponse response)
{
    TunnelConnectResult result {
        .resultCode = toNatTraversalResultCode(resultCode),
        .sysErrorCode = sysErrorCode,
        .stats = {
            .connectType = ConnectType::proxy,
            .responseTime = m_responseTimer.elapsed(),
            .remoteAddress = m_relayUrl.displayAddress().toStdString()},
    };
    if (httpStatusCode != http::StatusCode::undefined)
        result.stats.httpStatusCode = httpStatusCode;

    NX_VERBOSE(this, "%1. Received relay start session response %2, %3, %4",
        m_connectSessionId, resultCode, SystemError::toString(sysErrorCode),
        http::StatusCode::toString(httpStatusCode));

    decltype(m_handler) handler;
    handler.swap(m_handler);

    m_timeoutTimer.pleaseStopSync();

    if (!result.ok())
    {
        m_relayClient.reset();
        return handler(std::move(result));
    }

    if (response.sessionId != m_connectSessionId)
    {
        NX_VERBOSE(this, "%1. Switching to relay session id %2",
            m_connectSessionId, response.sessionId);
        m_connectSessionId = response.sessionId;
    }

    result.connection = std::make_unique<OutgoingTunnelConnection>(
        nx::utils::Url(response.actualRelayUrl),
        m_connectSessionId,
        std::move(m_relayClient));

    handler(std::move(result));
}

void Connector::connectTimedOut()
{
    onStartRelaySessionResponse(
        nx::cloud::relay::api::ResultCode::timedOut,
        SystemError::timedOut,
        http::StatusCode::undefined,
        nx::cloud::relay::api::CreateClientSessionResponse());
}

} // namespace nx::network::cloud::relay
