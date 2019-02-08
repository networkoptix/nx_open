#include "relay_api_client_over_http_get_post_tunnel.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>

namespace nx::cloud::relay::api::detail {

static constexpr char kHttpTunnelMessageBodySize[] = "10000000000";

ClientOverHttpGetPostTunnel::ClientOverHttpGetPostTunnel(
    const nx::utils::Url& baseUrl,
    ClientFeedbackFunction feedbackFunction)
    :
    base_type(baseUrl, std::move(feedbackFunction))
{
}

void ClientOverHttpGetPostTunnel::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    for (auto& tunnelCtx: m_tunnelsBeingEstablished)
    {
        if (tunnelCtx->httpClient)
            tunnelCtx->httpClient->bindToAioThread(aioThread);

        if (tunnelCtx->connection)
            tunnelCtx->connection->bindToAioThread(aioThread);
    }
}

void ClientOverHttpGetPostTunnel::beginListening(
    const std::string& peerName,
    BeginListeningHandler completionHandler)
{
    using namespace nx::network;

    const auto tunnelUrl =
        url::Builder(url()).appendPath(http::rest::substituteParameters(
            kServerTunnelPath,
            {peerName, std::string("1")}).c_str());

    post(
        [this, tunnelUrl, completionHandler = std::move(completionHandler)]() mutable
        {
            openDownChannel<detail::ServerTunnelContext>(
                tunnelUrl,
                std::move(completionHandler));
        });
}

void ClientOverHttpGetPostTunnel::openConnectionToTheTargetHost(
    const std::string& sessionId,
    OpenRelayConnectionHandler completionHandler)
{
    using namespace nx::network;

    const auto tunnelUrl =
        url::Builder(url()).appendPath(http::rest::substituteParameters(
            kClientGetPostTunnelPath,
            {sessionId, std::string("1")}).c_str());

    post(
        [this, tunnelUrl, completionHandler = std::move(completionHandler)]() mutable
        {
            openDownChannel<detail::ClientTunnelContext>(
                tunnelUrl,
                std::move(completionHandler));
        });
}

void ClientOverHttpGetPostTunnel::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_tunnelsBeingEstablished.clear();
}

template<typename TunnelContext, typename UserHandler>
void ClientOverHttpGetPostTunnel::openDownChannel(
    const nx::utils::Url& tunnelUrl,
    UserHandler completionHandler)
{
    m_tunnelsBeingEstablished.push_back(
        std::make_unique<TunnelContext>(
            std::move(completionHandler)));
    auto tunnelCtxIter = std::prev(m_tunnelsBeingEstablished.end());
    (*tunnelCtxIter)->tunnelUrl = tunnelUrl;

    (*tunnelCtxIter)->httpClient =
        std::make_unique<nx::network::http::AsyncClient>();
    (*tunnelCtxIter)->httpClient->bindToAioThread(getAioThread());
    (*tunnelCtxIter)->httpClient->setOnResponseReceived(
        std::bind(&ClientOverHttpGetPostTunnel::onDownChannelOpened, this, tunnelCtxIter));
    (*tunnelCtxIter)->httpClient->doGet(
        tunnelUrl,
        std::bind(&ClientOverHttpGetPostTunnel::cleanUpFailedTunnel, this, tunnelCtxIter));
}

void ClientOverHttpGetPostTunnel::onDownChannelOpened(
    Tunnels::iterator tunnelCtxIter)
{
    if (!(*tunnelCtxIter)->httpClient->hasRequestSucceeded())
        return cleanUpFailedTunnel(tunnelCtxIter);

    if (!(*tunnelCtxIter)->parseOpenDownChannelResponse(
            *(*tunnelCtxIter)->httpClient->response()))
    {
        return cleanUpFailedTunnel(tunnelCtxIter);
    }

    (*tunnelCtxIter)->connection = (*tunnelCtxIter)->httpClient->takeSocket();
    (*tunnelCtxIter)->httpClient.reset();

    openUpChannel(tunnelCtxIter);
}

void ClientOverHttpGetPostTunnel::openUpChannel(
    Tunnels::iterator tunnelCtxIter)
{
    using namespace std::placeholders;

    const auto openUpChannelRequest =
        prepareOpenUpChannelRequest(tunnelCtxIter);

    (*tunnelCtxIter)->serializedOpenUpChannelRequest =
        openUpChannelRequest.serialized();
    (*tunnelCtxIter)->connection->sendAsync(
        (*tunnelCtxIter)->serializedOpenUpChannelRequest,
        std::bind(&ClientOverHttpGetPostTunnel::handleOpenUpTunnelResult, this,
            tunnelCtxIter, _1, _2));
}

nx::network::http::Request ClientOverHttpGetPostTunnel::prepareOpenUpChannelRequest(
    Tunnels::iterator tunnelCtxIter)
{
    network::http::Request request;
    request.requestLine.method = network::http::Method::post;
    request.requestLine.version = network::http::http_1_1;
    request.requestLine.url = (*tunnelCtxIter)->tunnelUrl.path();

    request.headers.emplace("Content-Type", "application/octet-stream");
    request.headers.emplace("Content-Length", kHttpTunnelMessageBodySize);
    request.headers.emplace("Pragma", "no-cache");
    request.headers.emplace("Cache-Control", "no-cache");
    return request;
}

void ClientOverHttpGetPostTunnel::handleOpenUpTunnelResult(
    Tunnels::iterator tunnelCtxIter,
    SystemError::ErrorCode systemErrorCode,
    std::size_t /*bytesTransferreded*/)
{
    if (systemErrorCode != SystemError::noError)
        return cleanUpFailedTunnel(tunnelCtxIter);

    reportSuccess(tunnelCtxIter);
}

void ClientOverHttpGetPostTunnel::cleanUpFailedTunnel(
    Tunnels::iterator tunnelCtxIter)
{
    auto tunnelContext = std::move(*tunnelCtxIter);
    m_tunnelsBeingEstablished.erase(tunnelCtxIter);

    api::ResultCode resultCode = api::ResultCode::unknownError;
    if (tunnelContext->httpClient && tunnelContext->httpClient->response())
    {
        resultCode = fromHttpStatusCode(
            static_cast<network::http::StatusCode::Value>(
                tunnelContext->httpClient->response()->statusLine.statusCode));
    }

    giveFeedback(resultCode);

    tunnelContext->invokeUserHandler(resultCode);
}

void ClientOverHttpGetPostTunnel::reportSuccess(
    Tunnels::iterator tunnelCtxIter)
{
    auto tunnelContext = std::move(*tunnelCtxIter);
    m_tunnelsBeingEstablished.erase(tunnelCtxIter);

    tunnelContext->invokeUserHandler(api::ResultCode::ok);
}

} // namespace nx::cloud::relay::api::detail
