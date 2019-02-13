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
    m_httpConnection.setOnConnectionClosed(
        [this](auto reason) { handleConnectionClosure(reason); });

    bindToAioThread(m_httpConnection.getAioThread());
}

void TunnelValidator::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_httpConnection.bindToAioThread(aioThread);
    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

void TunnelValidator::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

void TunnelValidator::validate(
    network::http::tunneling::ValidateTunnelCompletionHandler handler)
{
    if (!relaySupportsKeepAlive())
    {
        return post([this, handler = std::move(handler)]()
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
        m_relayProtocolVersion = protocolHeaderIter->second.toStdString();
}

void TunnelValidator::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_httpConnection.pleaseStopSync();
    m_connection.reset();
}

bool TunnelValidator::relaySupportsKeepAlive() const
{
    if (m_relayProtocolVersion.empty() ||
        nx::utils::SoftwareVersion(m_relayProtocolVersion.c_str()) < nx::utils::SoftwareVersion(0, 1))
    {
        return false;
    }

    return true;
}

void TunnelValidator::processRelayNotification(network::http::Message message)
{
    using namespace network::http;

    NX_VERBOSE(this, lm("Received (%1) notification").args(
        message.type == MessageType::request
        ? message.request->requestLine.toString()
        : StringType()));

    NX_ASSERT(
        message.type == MessageType::request &&
        message.request->requestLine.method != OpenTunnelNotification::kHttpMethod);

    m_connection = m_httpConnection.takeSocket();

    if (m_handler)
        nx::utils::swapAndCall(m_handler, tunneling::ResultCode::ok);
}

void TunnelValidator::handleConnectionClosure(
    SystemError::ErrorCode /*reason*/)
{
    if (m_handler)
        nx::utils::swapAndCall(m_handler, network::http::tunneling::ResultCode::ioError);
}

} // namespace nx::cloud::relay::api::detail
