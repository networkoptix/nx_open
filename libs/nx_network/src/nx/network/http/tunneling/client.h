// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

#include "../http_types.h"
#include "abstract_tunnel_validator.h"
#include "detail/base_tunnel_client.h"

namespace nx::network::http::tunneling {

using TunnelValidatorFactoryFunc =
    std::function<std::unique_ptr<AbstractTunnelValidator>(
        std::unique_ptr<AbstractStreamSocket> /*tunnel*/,
        const Response&)>;

/**
 * Establishes connection to nx::network::http::tunneling::Server listening same request path.
 * For description of tunneling methods supported see nx::network::http::tunneling::Server.
 * The client can try to use different tunneling methods to find the one that works.
 */
class NX_NETWORK_API Client:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    /**
     * @param baseTunnelUrl Path must be equal to that passed to Server().
     * @param forcedTunnelType The tunnel type to use. The value MUST be understood by
     * detail::ClientFactory::instance().create.
     */
    Client(
        const nx::utils::Url& baseTunnelUrl,
        const std::string& userTag = "",
        std::optional<int> forcedTunnelType = std::nullopt,
        const ConnectOptions& options = {});

    virtual ~Client();

    void setTunnelValidatorFactory(TunnelValidatorFactoryFunc func);

    /**
     * If set to true then established connection (provided by Client::openTunnel),
     * that did not receive a single byte of data from remote host, is reported as
     * tunneling method failure and causes tunnel type switching.
     * See detail::ClientFactory for more information on tunnel types.
     * false by default.
     */
    void setConsiderSilentConnectionAsTunnelFailure(bool value);

    /**
     * If true, then the client may try to establish multiple tunnels of different types
     * simultaneously to find a working tunnel type faster.
     * By default, true.
     */
    void setConcurrentTunnelMethodsEnabled(bool value);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Set single timeout to be used as TCP connect and HTTP response receive timeouts.
     */
    void setTimeout(std::optional<std::chrono::milliseconds> timeout);

    /**
     * @param headers Added to each HTTP request this class generates.
     */
    void setCustomHeaders(HttpHeaders headers);

    /**
     * Initiate tunnel setup process. Starts HTTP message exchange with the tunneling server.
     */
    void openTunnel(OpenTunnelCompletionHandler completionHandler);

    /**
     * @return Last received HTTP response.
     */
    const Response& response() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct ClientContext
    {
        std::unique_ptr<detail::BaseTunnelClient> client;
        Response response;
        std::unique_ptr<AbstractTunnelValidator> validator;
        OpenTunnelResult result;
    };

    const nx::utils::Url m_baseTunnelUrl;
    std::vector<ClientContext> m_actualClients;
    std::size_t m_completedClients = 0;
    TunnelValidatorFactoryFunc m_validatorFactory;
    OpenTunnelCompletionHandler m_completionHandler;
    std::optional<std::chrono::milliseconds> m_timeout;
    bool m_isConsideringSilentConnectionATunnelFailure = false;
    bool m_concurrentTunnelMethodsEnabled = true;
    Response m_response;

    void handleOpenTunnelCompletion(ClientContext* client, OpenTunnelResult result);
    void handleTunnelValidationResult(ClientContext* ctx, ResultCode result);
    void reportResult(ClientContext* client, OpenTunnelResult result);
};

} // namespace nx::network::http::tunneling
