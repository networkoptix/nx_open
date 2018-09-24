#include "base_tunnel_client.h"

namespace nx_http::tunneling::detail {

BaseTunnelClient::BaseTunnelClient(
    const QUrl& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    m_baseTunnelUrl(baseTunnelUrl),
    m_clientFeedbackFunction(std::move(clientFeedbackFunction))
{
}

void BaseTunnelClient::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_httpClient)
        m_httpClient->bindToAioThread(aioThread);
    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

void BaseTunnelClient::stopWhileInAioThread()
{
    m_httpClient.reset();
    m_connection.reset();
}

void BaseTunnelClient::cleanupFailedTunnel()
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

void BaseTunnelClient::reportFailure(OpenTunnelResult result)
{
    if (m_clientFeedbackFunction)
        nx::utils::swapAndCall(m_clientFeedbackFunction, false);

    nx::utils::swapAndCall(m_completionHandler, std::move(result));
}

bool BaseTunnelClient::resetConnectionAttributes()
{
    return m_connection->setRecvTimeout(AbstractCommunicatingSocket::kNoTimeout)
        && m_connection->setSendTimeout(AbstractCommunicatingSocket::kNoTimeout);
}

void BaseTunnelClient::reportSuccess()
{
    nx::utils::swapAndCall(
        m_completionHandler,
        OpenTunnelResult{
            SystemError::noError,
            StatusCode::ok,
            std::exchange(m_connection, nullptr)});
}

} // namespace nx_http::tunneling::detail
