#include "relay_api_client_over_http_tunnel.h"

#include <chrono>

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/tunneling/client.h>
#include <nx/network/url/url_builder.h>

#include "detail/relay_tunnel_validator.h"
#include "relay_api_http_paths.h"

namespace nx::cloud::relay::api {

// TODO: Make this timeout configurable.
// And configure it properly when connecting!
static constexpr auto kTimeout = std::chrono::seconds(11);

ClientOverHttpTunnel::ClientOverHttpTunnel(
    const QUrl& baseUrl,
    ClientFeedbackFunction feedbackFunction)
    :
    base_type(baseUrl, std::move(feedbackFunction))
{
}

void ClientOverHttpTunnel::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    for (auto& tunnelClient: m_tunnelingClients)
        tunnelClient->bindToAioThread(aioThread);
}

void ClientOverHttpTunnel::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_tunnelingClients.clear();
}

void ClientOverHttpTunnel::beginListening(
    const std::string& peerName,
    BeginListeningHandler completionHandler)
{
    dispatch(
        [this, peerName, completionHandler = std::move(completionHandler)]() mutable
        {
            openServerTunnel(peerName, std::move(completionHandler));
        });
}

void ClientOverHttpTunnel::openConnectionToTheTargetHost(
    const std::string& sessionId,
    OpenRelayConnectionHandler completionHandler)
{
    dispatch(
        [this, sessionId, completionHandler = std::move(completionHandler)]() mutable
        {
            openClientTunnel(sessionId, std::move(completionHandler));
        });
}

void ClientOverHttpTunnel::openServerTunnel(
    const std::string& peerName,
    BeginListeningHandler completionHandler)
{
    NX_CRITICAL(isInSelfAioThread());

    const auto serverTunnelBaseUrl = network::url::Builder(url())
        .appendPath(nx_http::rest::substituteParameters(
            kServerTunnelBasePath, {peerName.c_str()})).toUrl();

    openTunnel(
        serverTunnelBaseUrl,
        [](auto&&... args) { return std::make_unique<detail::TunnelValidator>(
            std::forward<decltype(args)>(args)...); },
        [this, completionHandler = std::move(completionHandler)](
            auto&&... args) mutable
        {
            processServerTunnelResult(
                std::move(completionHandler),
                std::move(args)...);
        });
}

void ClientOverHttpTunnel::openTunnel(
    const QUrl& url,
    nx_http::tunneling::TunnelValidatorFactoryFunc tunnelValidatorFactoryFunc,
    ClientOverHttpTunnel::OpenTrafficRelayTunnelHandler handler)
{
    auto client = std::make_unique<nx_http::tunneling::Client>(url);
    if (tunnelValidatorFactoryFunc)
        client->setTunnelValidatorFactory(std::move(tunnelValidatorFactoryFunc));
    client->setCustomHeaders({{kNxProtocolHeader, kRelayProtocol}});
    client->setTimeout(kTimeout);
    auto clientPtr = client.get();
    m_tunnelingClients.push_back(std::move(client));

    clientPtr->openTunnel(
        [this, clientIter = std::prev(m_tunnelingClients.end()),
            handler = std::move(handler)](
                auto&&... args) mutable
        {
            auto tunnelingClient = std::move(*clientIter);
            m_tunnelingClients.erase(clientIter);

            handler(*tunnelingClient, std::move(args)...);
        });
}

void ClientOverHttpTunnel::processServerTunnelResult(
    BeginListeningHandler completionHandler,
    const nx_http::tunneling::Client& tunnelingClient,
    nx_http::tunneling::OpenTunnelResult result)
{
    const auto resultCode = getResultCode(result, tunnelingClient);
    if (resultCode != api::ResultCode::ok)
        return completionHandler(resultCode, BeginListeningResponse(), nullptr);

    BeginListeningResponse response;
    deserializeFromHeaders(tunnelingClient.response().headers, &response);
    completionHandler(
        api::ResultCode::ok,
        std::move(response),
        std::move(result.connection));
}

void ClientOverHttpTunnel::openClientTunnel(
    const std::string& sessionId,
    OpenRelayConnectionHandler completionHandler)
{
    const auto clientTunnelBaseUrl = network::url::Builder(url())
        .appendPath(nx_http::rest::substituteParameters(
            kClientTunnelBasePath, {sessionId.c_str()})).toUrl();

    openTunnel(
        clientTunnelBaseUrl,
        nullptr, //< Client tunnel verification is not supported yet.
        [this, completionHandler = std::move(completionHandler)](
            auto&&... args) mutable
        {
            processClientTunnelResult(
                std::move(completionHandler),
                std::move(args)...);
        });
}

void ClientOverHttpTunnel::processClientTunnelResult(
    OpenRelayConnectionHandler completionHandler,
    const nx_http::tunneling::Client& tunnelingClient,
    nx_http::tunneling::OpenTunnelResult result)
{
    completionHandler(
        getResultCode(result, tunnelingClient),
        std::move(result.connection));
}

api::ResultCode ClientOverHttpTunnel::getResultCode(
    const nx_http::tunneling::OpenTunnelResult& tunnelResult,
    const nx_http::tunneling::Client& tunnelingClient)
{
    if (tunnelResult.connection)
        return api::ResultCode::ok;

    return toUpgradeResultCode(
        tunnelResult.httpStatus,
        &tunnelingClient.response());
}

} // namespace nx::cloud::relay::api
