#include "base_tunnel_client.h"

namespace nx::network::http::tunneling::detail {

BaseTunnelClient::BaseTunnelClient(
    const nx::utils::Url& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    m_baseTunnelUrl(baseTunnelUrl),
    m_clientFeedbackFunction(std::move(clientFeedbackFunction))
{
}

void BaseTunnelClient::bindToAioThread(
    aio::AbstractAioThread* aioThread)
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

void BaseTunnelClient::cleanUpFailedTunnel()
{
    cleanUpFailedTunnel(m_httpClient.get());
}

void BaseTunnelClient::cleanUpFailedTunnel(AsyncClient* httpClient)
{
    OpenTunnelResult result;
    result.sysError = httpClient->lastSysErrorCode();
    if (httpClient->response())
    {
        result.httpStatus =
            (StatusCode::Value) httpClient->response()->statusLine.statusCode;
    }

    reportFailure(std::move(result));
}

void BaseTunnelClient::cleanUpFailedTunnel(
    SystemError::ErrorCode systemErrorCode)
{
    OpenTunnelResult result;
    result.sysError = systemErrorCode;
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
    return m_connection->setRecvTimeout(kNoTimeout)
        && m_connection->setSendTimeout(kNoTimeout);
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

} // namespace nx::network::http::tunneling::detail
