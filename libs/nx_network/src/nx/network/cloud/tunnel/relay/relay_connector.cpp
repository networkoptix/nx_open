// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_connector.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_notifications.h>
#include <nx/network/http/http_async_client.h>
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
    const hpm::api::ConnectResponse& response,
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    NX_VERBOSE(this, "%1. Starting relay connection to %2", m_connectSessionId, m_targetHostAddress);

    m_handler = std::move(handler);

    dispatch(
        [this, timeout, cloudConnectVersion = response.cloudConnectVersion]()
        {
            m_relayClient->setTimeout(timeout);
            m_responseTimer.restart();
            m_relayClient->startSession(
                m_connectSessionId,
                m_targetHostAddress.host.toString(),
                [this, cloudConnectVersion](
                    nx::cloud::relay::api::ResultCode resultCode,
                    nx::cloud::relay::api::CreateClientSessionResponse response)
                {
                    onStartRelaySessionResponse(
                        resultCode,
                        m_relayClient->prevRequestSysErrorCode(),
                        m_relayClient->prevRequestHttpStatusCode(),
                        std::move(response),
                        cloudConnectVersion);
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
    nx::cloud::relay::api::CreateClientSessionResponse response,
    nx::hpm::api::CloudConnectVersion cloudConnectVersion)
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

    m_timeoutTimer.pleaseStopSync();

    if (!result.ok())
    {
        m_relayClient.reset();
        nx::utils::swapAndCall(m_handler, std::move(result));
        return;
    }

    if (response.sessionId != m_connectSessionId)
    {
        NX_VERBOSE(this, "%1. Switching to relay session id %2",
            m_connectSessionId, response.sessionId);
        m_connectSessionId = response.sessionId;
    }
    if (cloudConnectVersion >=
        nx::hpm::api::CloudConnectVersion::clientSupportsRelayConnectionTesting)
    {
        NX_VERBOSE(this, "%1. Testing connection to %2", m_connectSessionId, m_targetHostAddress);
        return testConnection(
            [this, relayUrl = response.actualRelayUrl, result = std::move(result)](
                bool success) mutable
            {
                if (!success)
                {
                    result.resultCode =
                        nx::hpm::api::NatTraversalResultCode::endpointVerificationFailure;
                    nx::utils::swapAndCall(m_handler, std::move(result));
                    return;
                }

                result.connection = std::make_unique<OutgoingTunnelConnection>(
                    nx::utils::Url(relayUrl),
                    m_connectSessionId,
                    std::move(m_relayClient));

                nx::utils::swapAndCall(m_handler, std::move(result));
            });
    }

    result.connection = std::make_unique<OutgoingTunnelConnection>(
        nx::utils::Url(response.actualRelayUrl),
        m_connectSessionId,
        std::move(m_relayClient));

    nx::utils::swapAndCall(m_handler, std::move(result));
}

void Connector::testConnection(nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    m_connectionTestHandler = std::move(handler);
    m_relayClient->openConnectionToTheTargetHost(
        m_connectSessionId,
        [this](
            nx::cloud::relay::api::ResultCode resultCode,
            std::unique_ptr<network::AbstractStreamSocket> socket) mutable {
            if (resultCode != nx::cloud::relay::api::ResultCode::ok)
            {
                NX_VERBOSE(this, "Cannot open connection to target host: sessionId=%1, "
                    "resultCode=%2", m_connectSessionId, resultCode);
                return nx::utils::swapAndCall(m_connectionTestHandler, false);
            }

            m_httpPipeline = std::make_unique<nx::network::http::AsyncMessagePipeline>(
                std::move(socket));
            m_httpPipeline->registerCloseHandler(
                [this](auto&&... args) { onTestConnectionClosed(std::move(args)...); });
            m_httpPipeline->setMessageHandler(
                [this](auto&&... args) { onTestConnectionResponse(std::move(args)...); });
            m_httpPipeline->startReadingConnection();

            nx::cloud::relay::api::TestConnectionNotification request(
                m_targetHostAddress.host.toString());
            m_expectedConnectionTestId = request.testId();

            m_httpPipeline->sendMessage(request.toHttpMessage());
        },
        nx::network::http::tunneling::ConnectOptions{
            {nx::network::http::tunneling::kConnectOptionRunConnectionTest, "1"}});
}

void Connector::onTestConnectionResponse(nx::network::http::Message message)
{
    nx::cloud::relay::api::TestConnectionNotificationResponse response;
    if (!response.parse(message))
    {
        NX_VERBOSE(this, "Unexpected response: sessionId=%1, response=%2",
            m_connectSessionId, message.response->statusLine.toString());
        return;
    }

    if (response.statusCode() != nx::network::http::StatusCode::ok)
    {
        NX_VERBOSE(this, "Connection test failed: sessionId=%1, status=%2",
            m_connectSessionId, message.response->statusLine.toString());
        nx::utils::swapAndCall(m_connectionTestHandler, false);
        return;
    }

    if (response.testId().empty() || response.testId() != m_expectedConnectionTestId)
    {
        NX_VERBOSE(this, "Connection test failed: sessionId=%1, testId=%2, expected=%3",
            m_connectSessionId, response.testId(), m_expectedConnectionTestId);
        nx::utils::swapAndCall(m_connectionTestHandler, false);
        return;
    }

    NX_VERBOSE(this, "Connection test succeeded: sessionId=%1", m_connectSessionId);

    decltype(m_connectionTestHandler) handler;
    handler.swap(m_connectionTestHandler);
    m_httpPipeline.reset();
    handler(true);
}

void Connector::onTestConnectionClosed(
    SystemError::ErrorCode closeReason,
    bool /*connectionDestroyed*/)
{
    NX_VERBOSE(this, "Connection %1->%2 is closed with result %3",
        m_httpPipeline->socket()->getLocalAddress(), m_httpPipeline->socket()->getForeignAddress(),
        SystemError::toString(closeReason));

    m_httpPipeline.reset();

    decltype(m_connectionTestHandler) handler;
    handler.swap(m_connectionTestHandler);
    if (handler)
        handler(false);
}

void Connector::connectTimedOut()
{
    onStartRelaySessionResponse(
        nx::cloud::relay::api::ResultCode::timedOut,
        SystemError::timedOut,
        http::StatusCode::undefined,
        nx::cloud::relay::api::CreateClientSessionResponse(),
        nx::hpm::api::CloudConnectVersion::initial);
}

} // namespace nx::network::cloud::relay
