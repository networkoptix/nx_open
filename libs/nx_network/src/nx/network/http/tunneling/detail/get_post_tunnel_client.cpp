// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "get_post_tunnel_client.h"

#include <nx/network/url/url_builder.h>

#include "../../rest/http_rest_client.h"
#include "request_paths.h"

namespace nx::network::http::tunneling::detail {

GetPostTunnelClient::GetPostTunnelClient(
    const nx::utils::Url& baseTunnelUrl,
    const ConnectOptions& options,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    base_type(baseTunnelUrl, std::move(clientFeedbackFunction)),
    m_isConnectionTestRequested(options.contains(kConnectOptionRunConnectionTest))
{
}

void GetPostTunnelClient::setTimeout(std::optional<std::chrono::milliseconds> timeout)
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
            {std::string("1")}));
    if (m_isConnectionTestRequested)
        m_tunnelUrl.setQuery(QString(kConnectOptionRunConnectionTest) + "=1");
    m_completionHandler = std::move(completionHandler);

    post([this]() { openDownChannel(); });
}

const Response& GetPostTunnelClient::response() const
{
    return m_openTunnelResponse;
}

void GetPostTunnelClient::openDownChannel()
{
    NX_VERBOSE(this, "%1. Opening down channel", m_tunnelUrl);

    m_httpClient = std::make_unique<AsyncClient>(ssl::kDefaultCertificateCheck);
    m_httpClient->setAdditionalHeaders(customHeaders());
    if (m_timeout)
    {
        m_httpClient->setSendTimeout(*m_timeout);
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
    {
        NX_VERBOSE(this, "%1. Open down channel failed with %2",
            m_tunnelUrl,
            m_httpClient->response() ? m_httpClient->response()->statusLine.toString() : "I/O error");
        auto httpClient = std::exchange(m_httpClient, nullptr);
        return cleanUpFailedTunnel(httpClient.get());
    }

    NX_VERBOSE(this, "%1. Open down channel succeeded", m_tunnelUrl);

    m_connection = m_httpClient->takeSocket();
    m_openTunnelResponse = std::move(*m_httpClient->response());
    m_httpClient.reset();

    openUpChannel();
}

void GetPostTunnelClient::openUpChannel()
{
    using namespace std::placeholders;

    NX_VERBOSE(this, "%1. Opening up channel", m_tunnelUrl);

    const auto openUpChannelRequest = prepareOpenUpChannelRequest();

    m_serializedOpenUpChannelRequest = openUpChannelRequest.serialized();
    m_connection->sendAsync(
        &m_serializedOpenUpChannelRequest,
        std::bind(&GetPostTunnelClient::handleOpenUpTunnelResult, this, _1, _2));
}

nx::network::http::Request GetPostTunnelClient::prepareOpenUpChannelRequest()
{
    network::http::Request request;
    request.requestLine.method = network::http::Method::post;
    request.requestLine.version = network::http::http_1_1;
    request.requestLine.url = m_tunnelUrl.path();
    request.requestLine.url.setQuery(m_tunnelUrl.query());

    request.headers.emplace("Host", url::getEndpoint(m_tunnelUrl).toString());
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
    NX_VERBOSE(this, "%1. Open up channel completed with result %2",
        m_tunnelUrl, SystemError::toString(systemErrorCode));

    if (systemErrorCode != SystemError::noError)
        return reportFailure(OpenTunnelResult(systemErrorCode));

    if (!resetConnectionAttributes())
        return reportFailure(OpenTunnelResult(SystemError::getLastOSErrorCode()));

    reportSuccess();
}

} // namespace nx::network::http::tunneling::detail
