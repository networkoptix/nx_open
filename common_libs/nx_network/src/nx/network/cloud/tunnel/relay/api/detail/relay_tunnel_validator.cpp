#include "relay_tunnel_validator.h"

#include <nx/utils/log/log.h>
#include <nx/utils/software_version.h>

#include "../relay_api_notifications.h"
#include "../relay_api_http_paths.h"

namespace nx::cloud::relay::api::detail {

TunnelValidator::TunnelValidator(
    std::unique_ptr<AbstractStreamSocket> connection,
    const nx_http::Response& response)
    :
    m_httpConnection(this, std::move(connection))
{
    fetchProtocolVersion(response);

    m_httpConnection.setMessageHandler(
        [this](auto message) { processRelayNotification(std::move(message)); });

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
    nx_http::tunneling::ValidateTunnelCompletionHandler handler)
{
    if (!relaySupportsKeepAlive())
    {
        return post([handler = std::move(handler)]()
            { handler(nx_http::tunneling::ResultCode::ok); });
    }

    m_handler = std::move(handler);
    m_httpConnection.startReadingConnection(
        m_timeout ? boost::make_optional(*m_timeout) : boost::none);
}

std::unique_ptr<AbstractStreamSocket> TunnelValidator::takeConnection()
{
    if (m_connection)
        return std::exchange(m_connection, nullptr);

    return m_httpConnection.takeSocket();
}

void TunnelValidator::closeConnection(
    SystemError::ErrorCode closeReason,
    nx_http::AsyncMessagePipeline* /*connection*/)
{
    handleConnectionClosure(closeReason);
}

void TunnelValidator::fetchProtocolVersion(
    const nx_http::Response& response)
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

void TunnelValidator::processRelayNotification(nx_http::Message message)
{
    using namespace nx_http;

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
        nx::utils::swapAndCall(m_handler, nx_http::tunneling::ResultCode::ioError);
}

} // namespace nx::cloud::relay::api::detail
