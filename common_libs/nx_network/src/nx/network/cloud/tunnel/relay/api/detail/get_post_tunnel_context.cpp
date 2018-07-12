#include "get_post_tunnel_context.h"

namespace nx::cloud::relay::api::detail {

ServerTunnelContext::ServerTunnelContext(
    BeginListeningHandler userHandler)
    :
    m_userHandler(std::move(userHandler))
{
}

bool ServerTunnelContext::parseOpenDownChannelResponse(
    const network::http::Response& response)
{
    return deserializeFromHeaders(
        response.headers,
        &m_beginListeningResponse);
}

void ServerTunnelContext::invokeUserHandler(api::ResultCode resultCode)
{
    m_userHandler(
        resultCode,
        std::move(m_beginListeningResponse),
        std::move(connection));
}

//-------------------------------------------------------------------------------------------------

ClientTunnelContext::ClientTunnelContext(
    OpenRelayConnectionHandler userHandler)
    :
    m_userHandler(std::move(userHandler))
{
}

bool ClientTunnelContext::parseOpenDownChannelResponse(
    const network::http::Response& /*response*/)
{
    return true;
}

void ClientTunnelContext::invokeUserHandler(api::ResultCode resultCode)
{
    m_userHandler(
        resultCode,
        std::move(connection));
}

} // namespace nx::cloud::relay::api::detail
