// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_tunnel_client.h"

#include <nx/utils/log/format.h>

namespace nx::network::http::tunneling {

std::string toString(ResultCode code)
{
    switch (code)
    {
        case ResultCode::ok: return "ok";
        case ResultCode::httpUpgradeFailed: return "httpUpgradeFailed";
        case ResultCode::ioError: return "ioError";
    }

    return "unknown";
}

OpenTunnelResult::OpenTunnelResult(std::unique_ptr<AbstractStreamSocket> connection):
    connection(std::move(connection))
{
}

OpenTunnelResult::OpenTunnelResult(SystemError::ErrorCode sysError):
    resultCode(ResultCode::ioError),
    sysError(sysError)
{
}

bool OpenTunnelResult::ok() const
{
    return connection != nullptr;
}

std::string OpenTunnelResult::toString() const
{
    return nx::format("Result %1, sys error %2, HTTP status %3")
        .args(tunneling::toString(resultCode), SystemError::toString(sysError),
            httpStatus ? http::StatusCode::toString(*httpStatus) : "-").toStdString();
}

//-------------------------------------------------------------------------------------------------

namespace detail {

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

ClientFeedbackFunction BaseTunnelClient::takeFeedbackFunction()
{
    return std::exchange(m_clientFeedbackFunction, nullptr);
}

void BaseTunnelClient::setCustomHeaders(HttpHeaders headers)
{
    m_customHeaders = std::move(headers);
}

const HttpHeaders& BaseTunnelClient::customHeaders() const
{
    return m_customHeaders;
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
    result.resultCode = result.sysError == SystemError::noError
        ? ResultCode::httpUpgradeFailed : ResultCode::ioError;
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
    result.resultCode = ResultCode::ioError;
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
    OpenTunnelResult result(std::exchange(m_connection, nullptr));
    result.httpStatus = (StatusCode::Value) response().statusLine.statusCode;

    nx::utils::swapAndCall(
        m_completionHandler,
        std::move(result));
}

} // namespace detail

} // namespace nx::network::http::tunneling
