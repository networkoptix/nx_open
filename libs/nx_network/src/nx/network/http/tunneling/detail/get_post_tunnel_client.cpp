#include "get_post_tunnel_client.h"

#include <nx/network/url/url_builder.h>

#include "request_paths.h"
#include "../../rest/http_rest_client.h"

namespace nx::network::http::tunneling::detail {

GetPostTunnelClient::GetPostTunnelClient(
    const nx::utils::Url& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    base_type(baseTunnelUrl, std::move(clientFeedbackFunction))
{
}

void GetPostTunnelClient::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
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

void GetPostTunnelClient::openDownChannel()
{
    m_httpClient = std::make_unique<nx::network::http::AsyncClient>();
    m_httpClient->setAdditionalHeaders(customHeaders());
    if (m_timeout)
    {
        m_httpClient->setResponseReadTimeout(*m_timeout);
        m_httpClient->setMessageBodyReadTimeout(*m_timeout);
    }

    m_httpClient->bindToAioThread(getAioThread());
    m_httpClient->setOnResponseReceived(
        std::bind(&GetPostTunnelClient::onDownChannelOpened, this));
    m_httpClient->doGet(
        m_tunnelUrl,
        [this]() { cleanUpFailedTunnel(); });
}

void GetPostTunnelClient::onDownChannelOpened()
{
    if (!m_httpClient->hasRequestSucceeded())
        return cleanUpFailedTunnel();

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
        return reportFailure(OpenTunnelResult(systemErrorCode));

    if (!resetConnectionAttributes())
        return reportFailure(OpenTunnelResult(SystemError::getLastOSErrorCode()));

    reportSuccess();
}

} // namespace nx::network::http::tunneling::detail
