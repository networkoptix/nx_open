#include "get_post_tunnel_client.h"

#include <nx/network/url/url_builder.h>

#include "request_paths.h"
#include "../../rest/http_rest_client.h"

namespace nx::network::http::tunneling::detail {

GetPostTunnelClient::GetPostTunnelClient(
    const nx::utils::Url& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    m_baseTunnelUrl(baseTunnelUrl),
    m_clientFeedbackFunction(std::move(clientFeedbackFunction))
{
}

void GetPostTunnelClient::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_httpClient)
        m_httpClient->bindToAioThread(aioThread);
    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

void GetPostTunnelClient::openTunnel(
    OpenTunnelCompletionHandler completionHandler)
{
    using namespace nx::network;

    m_tunnelUrl = url::Builder(m_baseTunnelUrl).appendPath(
        http::rest::substituteParameters(
            kGetPostTunnelPath,
            {std::string("1")}).c_str());
    m_completionHandler = std::move(completionHandler);

    post([this]() { openDownChannel(); });
}

const Response& GetPostTunnelClient::response() const
{
    return m_openTunnelResponse;
}

void GetPostTunnelClient::stopWhileInAioThread()
{
    m_httpClient.reset();
    m_connection.reset();
}

void GetPostTunnelClient::openDownChannel()
{
    m_httpClient = std::make_unique<nx::network::http::AsyncClient>();
    m_httpClient->bindToAioThread(getAioThread());
    m_httpClient->setOnResponseReceived(
        std::bind(&GetPostTunnelClient::onDownChannelOpened, this));
    // TODO: HTTP method should be configurable.
    m_httpClient->doGet(
        m_tunnelUrl,
        std::bind(&GetPostTunnelClient::cleanupFailedTunnel, this));
}

void GetPostTunnelClient::onDownChannelOpened()
{
    if (!m_httpClient->hasRequestSucceeded())
        return cleanupFailedTunnel();

    m_connection = m_httpClient->takeSocket();
    m_openTunnelResponse = std::move(*m_httpClient->response());
    m_httpClient.reset();

    openUpChannel();
}

void GetPostTunnelClient::openUpChannel()
{
    using namespace std::placeholders;

    const auto openUpChannelRequest = prepareOpenUpChannelRequest();

    m_serializedOpenUpChannelRequest = openUpChannelRequest.serialized();
    m_connection->sendAsync(
        m_serializedOpenUpChannelRequest,
        std::bind(&GetPostTunnelClient::handleOpenUpTunnelResult, this,
            _1, _2));
}

nx::network::http::Request GetPostTunnelClient::prepareOpenUpChannelRequest()
{
    network::http::Request request;
    request.requestLine.method = network::http::Method::post;
    request.requestLine.version = network::http::http_1_1;
    request.requestLine.url = m_tunnelUrl.path();

    request.headers.emplace("Content-Type", "application/octet-stream");
    request.headers.emplace("Content-Length", "10000000000");
    request.headers.emplace("Pragma", "no-cache");
    request.headers.emplace("Cache-Control", "no-cache");
    return request;
}

void GetPostTunnelClient::handleOpenUpTunnelResult(
    SystemError::ErrorCode systemErrorCode,
    std::size_t /*bytesTransferred*/)
{
    if (systemErrorCode != SystemError::noError)
    {
        return reportFailure({
            systemErrorCode,
            StatusCode::serviceUnavailable,
            nullptr});
    }

    if (!resetConnectionAttributes())
    {
        return reportFailure({
            SystemError::getLastOSErrorCode(),
            StatusCode::serviceUnavailable,
            nullptr});
    }

    reportSuccess();
}

void GetPostTunnelClient::cleanupFailedTunnel()
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

void GetPostTunnelClient::reportFailure(OpenTunnelResult result)
{
    if (m_clientFeedbackFunction)
        nx::utils::swapAndCall(m_clientFeedbackFunction, false);

    nx::utils::swapAndCall(m_completionHandler, std::move(result));
}

bool GetPostTunnelClient::resetConnectionAttributes()
{
    return m_connection->setRecvTimeout(kNoTimeout)
        && m_connection->setSendTimeout(kNoTimeout);
}

void GetPostTunnelClient::reportSuccess()
{
    nx::utils::swapAndCall(
        m_completionHandler,
        OpenTunnelResult{
            SystemError::noError,
            StatusCode::ok,
            std::exchange(m_connection, nullptr)});
}

} // namespace nx::network::http::tunneling::detail
