#include "basic_tunnel_client.h"

namespace nx::network::http::tunneling::detail {

BasicTunnelClient::BasicTunnelClient(
    const nx::utils::Url& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    m_baseTunnelUrl(baseTunnelUrl),
    m_clientFeedbackFunction(std::move(clientFeedbackFunction))
{
}

void BasicTunnelClient::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_httpClient)
        m_httpClient->bindToAioThread(aioThread);
    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

void BasicTunnelClient::stopWhileInAioThread()
{
    m_httpClient.reset();
    m_connection.reset();
}

void BasicTunnelClient::cleanupFailedTunnel()
{
    OpenTunnelResult result;
    result.sysError = m_httpClient->lastSysErrorCode();
    if (m_httpClient->response())
    {
        result.httpStatus =
            (StatusCode::Value) m_httpClient->response()->statusLine.statusCode;
    }
    m_httpClient.reset();

    reportFailure(std::move(result));
}

void BasicTunnelClient::reportFailure(OpenTunnelResult result)
{
    if (m_clientFeedbackFunction)
        nx::utils::swapAndCall(m_clientFeedbackFunction, false);

    nx::utils::swapAndCall(m_completionHandler, std::move(result));
}

bool BasicTunnelClient::resetConnectionAttributes()
{
    return m_connection->setRecvTimeout(kNoTimeout)
        && m_connection->setSendTimeout(kNoTimeout);
}

void BasicTunnelClient::reportSuccess()
{
    nx::utils::swapAndCall(
        m_completionHandler,
        OpenTunnelResult{
            SystemError::noError,
            StatusCode::ok,
            std::exchange(m_connection, nullptr)});
}

} // namespace nx::network::http::tunneling::detail
