// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_tunnel_validator.h"

#include <nx/utils/log/log.h>
#include <nx/utils/software_version.h>

#include "../relay_api_notifications.h"
#include "../relay_api_http_paths.h"

namespace nx::cloud::relay::api::detail {

TunnelValidator::TunnelValidator(
    std::unique_ptr<network::AbstractStreamSocket> connection,
    const network::http::Response& response)
    :
    m_httpConnection(std::move(connection))
{
    fetchProtocolVersion(response);

    m_httpConnection.setMessageHandler(
        [this](auto message) { processRelayNotification(std::move(message)); });
    m_httpConnection.registerCloseHandler(
        [this](auto reason, auto /*registerCloseHandler*/) { handleConnectionClosure(reason); });

    bindToAioThread(m_httpConnection.getAioThread());
}

void TunnelValidator::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_httpConnection.bindToAioThread(aioThread);
    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

void TunnelValidator::setTimeout(std::optional<std::chrono::milliseconds> timeout)
{
    m_timeout = timeout;
}

void TunnelValidator::validate(
    network::http::tunneling::ValidateTunnelCompletionHandler handler)
{
    if (!relaySupportsKeepAlive())
    {
        return post([handler = std::move(handler)]()
            { handler(network::http::tunneling::ResultCode::ok); });
    }

    m_handler = std::move(handler);
    m_httpConnection.startReadingConnection(m_timeout);
}

std::unique_ptr<network::AbstractStreamSocket> TunnelValidator::takeConnection()
{
    if (m_connection)
        return std::exchange(m_connection, nullptr);

    return m_httpConnection.takeSocket();
}

void TunnelValidator::fetchProtocolVersion(
    const network::http::Response& response)
{
    auto protocolHeaderIter = response.headers.find(kNxProtocolHeader);
    if (protocolHeaderIter != response.headers.end())
    {
        nx::network::http::MimeProtoVersion proto;
        if (proto.parse(protocolHeaderIter->second))
            m_relayProtocol = std::move(proto);
    }
}

void TunnelValidator::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_httpConnection.pleaseStopSync();
    m_connection.reset();
}

bool TunnelValidator::relaySupportsKeepAlive() const
{
    if (m_relayProtocol &&
        nx::utils::SoftwareVersion(m_relayProtocol->version) < nx::utils::SoftwareVersion(0, 1))
    {
        return false;
    }

    // NOTE: Considering empty protocol to be result of a firewall misbehavior.
    // Currently, every running relay supports keep-alive.

    return true;
}

void TunnelValidator::processRelayNotification(network::http::Message message)
{
    using namespace network::http;

    auto resultCode = tunneling::ResultCode::ok;

    if (message.type == MessageType::request &&
        message.request->requestLine.method != OpenTunnelNotification::kHttpMethod)
    {
        NX_VERBOSE(this, "Received (%1) relay notification",
            message.request->requestLine.toString());

        resultCode = tunneling::ResultCode::ok;
    }
    else
    {
        NX_VERBOSE(this, "Received unexpected HTTP message: \r\n%1",
            message.toString());

        resultCode = tunneling::ResultCode::ioError;
    }

    m_connection = m_httpConnection.takeSocket();

    if (m_handler)
        nx::utils::swapAndCall(m_handler, resultCode);
}

void TunnelValidator::handleConnectionClosure(
    SystemError::ErrorCode /*reason*/)
{
    if (m_handler)
        nx::utils::swapAndCall(m_handler, network::http::tunneling::ResultCode::ioError);
}

} // namespace nx::cloud::relay::api::detail
