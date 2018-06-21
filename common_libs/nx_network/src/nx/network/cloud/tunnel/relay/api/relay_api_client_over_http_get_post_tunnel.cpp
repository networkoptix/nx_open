#include "relay_api_client_over_http_get_post_tunnel.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>

namespace nx::cloud::relay::api {

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
        if (tunnelCtx.httpClient)
            tunnelCtx.httpClient->bindToAioThread(aioThread);

        if (tunnelCtx.connection)
            tunnelCtx.connection->bindToAioThread(aioThread);
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
            openDownChannel(tunnelUrl, std::move(completionHandler));
        });
}

//void ClientOverHttpGetPostTunnel::openConnectionToTheTargetHost(
//    const std::string& sessionId,
//    OpenRelayConnectionHandler completionHandler)
//{
//    // TODO
//
//    post(
//        [this, completionHandler = std::move(completionHandler)]
//        {
//            completionHandler(
//                api::ResultCode::unknownError,
//                nullptr);
//        });
//}

void ClientOverHttpGetPostTunnel::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_tunnelsBeingEstablished.clear();
}

void ClientOverHttpGetPostTunnel::openDownChannel(
    const nx::utils::Url& tunnelUrl,
    BeginListeningHandler completionHandler)
{
    m_tunnelsBeingEstablished.push_back(
        {tunnelUrl, std::move(completionHandler)});
    auto tunnelCtxIter = std::prev(m_tunnelsBeingEstablished.end());

    tunnelCtxIter->httpClient =
        std::make_unique<nx::network::http::AsyncClient>();
    tunnelCtxIter->httpClient->bindToAioThread(getAioThread());
    tunnelCtxIter->httpClient->setOnResponseReceived(
        std::bind(&ClientOverHttpGetPostTunnel::onDownChannelOpened, this, tunnelCtxIter));
    tunnelCtxIter->httpClient->doGet(
        tunnelUrl,
        std::bind(&ClientOverHttpGetPostTunnel::cleanupFailedTunnel, this, tunnelCtxIter));
}

void ClientOverHttpGetPostTunnel::onDownChannelOpened(
    std::list<TunnelContext>::iterator tunnelCtxIter)
{
    if (!tunnelCtxIter->httpClient->hasRequestSucceeded())
        return cleanupFailedTunnel(tunnelCtxIter);

    // TODO: Checking response.

    if (!deserializeFromHeaders(
            tunnelCtxIter->httpClient->response()->headers,
            &tunnelCtxIter->beginListeningResponse))
    {
        return cleanupFailedTunnel(tunnelCtxIter);
    }

    tunnelCtxIter->connection = tunnelCtxIter->httpClient->takeSocket();
    tunnelCtxIter->httpClient.reset();

    openUpChannel(tunnelCtxIter);
}

void ClientOverHttpGetPostTunnel::openUpChannel(
    std::list<TunnelContext>::iterator tunnelCtxIter)
{
    using namespace std::placeholders;

    const auto openUpChannelRequest =
        prepareOpenUpChannelRequest(tunnelCtxIter);

    tunnelCtxIter->serializedOpenUpChannelRequest =
        openUpChannelRequest.serialized();
    tunnelCtxIter->connection->sendAsync(
        tunnelCtxIter->serializedOpenUpChannelRequest,
        std::bind(&ClientOverHttpGetPostTunnel::handleOpenUpTunnelResult, this,
            tunnelCtxIter, _1, _2));
}

nx::network::http::Request ClientOverHttpGetPostTunnel::prepareOpenUpChannelRequest(
    std::list<TunnelContext>::iterator tunnelCtxIter)
{
    network::http::Request request;
    request.requestLine.method = network::http::Method::post;
    request.requestLine.version = network::http::http_1_1;
    request.requestLine.url = tunnelCtxIter->tunnelUrl.path();

    request.headers.emplace("Content-Type", "application/octet-stream");
    request.headers.emplace("Content-Length", "10000000000");
    request.headers.emplace("Pragma", "no-cache");
    request.headers.emplace("Cache-Control", "no-cache");
    return request;
}

void ClientOverHttpGetPostTunnel::handleOpenUpTunnelResult(
    std::list<TunnelContext>::iterator tunnelCtxIter,
    SystemError::ErrorCode systemErrorCode,
    std::size_t bytesTransferreded)
{
    if (systemErrorCode != SystemError::noError)
        return cleanupFailedTunnel(tunnelCtxIter);

    reportSuccess(tunnelCtxIter);
}

void ClientOverHttpGetPostTunnel::cleanupFailedTunnel(
    std::list<TunnelContext>::iterator tunnelCtxIter)
{
    auto tunnelContext = std::move(*tunnelCtxIter);
    m_tunnelsBeingEstablished.erase(tunnelCtxIter);

    api::ResultCode resultCode = api::ResultCode::unknownError;
    // TODO: Clarify resultCode.

    giveFeedback(resultCode);

    tunnelContext.userHandler(
        resultCode,
        BeginListeningResponse(),
        nullptr);
}

void ClientOverHttpGetPostTunnel::reportSuccess(
    std::list<TunnelContext>::iterator tunnelCtxIter)
{
    auto tunnelContext = std::move(*tunnelCtxIter);
    m_tunnelsBeingEstablished.erase(tunnelCtxIter);

    tunnelContext.userHandler(
        api::ResultCode::ok,
        std::move(tunnelContext.beginListeningResponse),
        std::move(tunnelContext.connection));
}

} // namespace nx::cloud::relay::api
