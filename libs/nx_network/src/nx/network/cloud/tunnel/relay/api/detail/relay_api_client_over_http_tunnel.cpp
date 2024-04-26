// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_api_client_over_http_tunnel.h"

#include <chrono>

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/tunneling/client.h>
#include <nx/network/url/url_builder.h>

#include "relay_tunnel_validator.h"
#include "../relay_api_http_paths.h"

namespace nx::cloud::relay::api::detail {

ClientOverHttpTunnel::ClientOverHttpTunnel(
    const nx::utils::Url& baseUrl,
    std::optional<int> forcedHttpTunnelType)
    :
    base_type(baseUrl),
    m_forcedHttpTunnelType(forcedHttpTunnelType)
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
        .appendPath(network::http::rest::substituteParameters(
            kServerTunnelBasePath, {peerName})).toUrl();

    openTunnel(
        std::make_unique<network::http::tunneling::Client>(
            serverTunnelBaseUrl,
            "",
            m_forcedHttpTunnelType),
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
    std::unique_ptr<network::http::tunneling::Client> client,
    network::http::tunneling::TunnelValidatorFactoryFunc tunnelValidatorFactoryFunc,
    ClientOverHttpTunnel::OpenTrafficRelayTunnelHandler handler)
{
    if (tunnelValidatorFactoryFunc)
        client->setTunnelValidatorFactory(std::move(tunnelValidatorFactoryFunc));
    client->setCustomHeaders({{kNxProtocolHeader, kRelayProtocol}});
    if (timeout())
        client->setTimeout(*timeout());
    auto clientPtr = client.get();
    m_tunnelingClients.push_back(std::move(client));

    clientPtr->openTunnel(
        [this, clientIter = std::prev(m_tunnelingClients.end()),
            handler = std::move(handler)](
                auto&&... args) mutable
        {
            auto tunnelingClient = std::move(*clientIter);
            m_tunnelingClients.erase(clientIter);

            handler(*tunnelingClient, std::forward<decltype(args)>(args)...);
        });
}

void ClientOverHttpTunnel::processServerTunnelResult(
    BeginListeningHandler completionHandler,
    const network::http::tunneling::Client& tunnelingClient,
    network::http::tunneling::OpenTunnelResult result)
{
    NX_VERBOSE(this, "Tunnel to %1 completed with result %2", url(), result);

    setPrevRequestFields(result);

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
        .appendPath(network::http::rest::substituteParameters(
            kClientTunnelBasePath, {sessionId})).toUrl();

    auto tunnellingClient = std::make_unique<network::http::tunneling::Client>(
        clientTunnelBaseUrl,
        "",
        m_forcedHttpTunnelType);
    tunnellingClient->setConsiderSilentConnectionAsTunnelFailure(true);

    openTunnel(
        std::move(tunnellingClient),
        nullptr, //< Client tunnel verification is not supported yet.
        [this, completionHandler = std::move(completionHandler)](
            auto&&... args) mutable
        {
            processClientTunnelResult(
                std::move(completionHandler),
                std::forward<decltype(args)>(args)...);
        });
}

void ClientOverHttpTunnel::processClientTunnelResult(
    OpenRelayConnectionHandler completionHandler,
    const network::http::tunneling::Client& tunnelingClient,
    network::http::tunneling::OpenTunnelResult result)
{
    setPrevRequestFields(result);
    const auto resultCode = getResultCode(result, tunnelingClient);
    completionHandler(resultCode, std::move(result.connection));
}

void ClientOverHttpTunnel::setPrevRequestFields(
    const network::http::tunneling::OpenTunnelResult& tunnelResult)
{
    setPrevRequestSysErrorCode(tunnelResult.sysError);
    setPrevRequestHttpStatusCode(
        tunnelResult.httpStatus
        ? *tunnelResult.httpStatus
        : nx::network::http::StatusCode::undefined);
}

api::ResultCode ClientOverHttpTunnel::getResultCode(
    const network::http::tunneling::OpenTunnelResult& tunnelResult,
    const network::http::tunneling::Client& tunnelingClient)
{
    if (tunnelResult.connection)
        return api::ResultCode::ok;

    if (tunnelResult.sysError != SystemError::noError)
    {
        if (auto rc = toUpgradeResultCode(tunnelResult.sysError, &tunnelingClient.response());
            rc != api::ResultCode::ok)
        {
            return rc;
        }
    }

    if (tunnelResult.httpStatus)
    {
        if (auto rc = fromHttpStatusCode(*tunnelResult.httpStatus); rc != api::ResultCode::ok)
            return rc;
    }

    return api::ResultCode::unknownError;
}

} // namespace nx::cloud::relay::api::detail
