// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <memory>
#include <tuple>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

#include "abstract_tunnel_validator.h"
#include "detail/base_tunnel_client.h"
#include "../http_types.h"

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
        std::optional<int> forcedTunnelType = std::nullopt);

    virtual ~Client();

    void setTunnelValidatorFactory(TunnelValidatorFactoryFunc func);

    /**
     * If set to true then established connection (provided by Client::openTunnel),
     * that did not receive a single byte of data from remote host, is reported as
     * tunnelling method failure and causes tunnel type switching.
     * See detail::ClientFactory for more information on tunnel types.
     * false by default.
     */
    void setConsiderSilentConnectionAsTunnelFailure(bool value);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void setTimeout(std::optional<std::chrono::milliseconds> timeout);

    void setCustomHeaders(HttpHeaders headers);

    void openTunnel(OpenTunnelCompletionHandler completionHandler);

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
    Response m_response;

    void handleOpenTunnelCompletion(ClientContext* client, OpenTunnelResult result);
    void handleTunnelValidationResult(ClientContext* ctx, ResultCode result);
    void reportResult(ClientContext* client, OpenTunnelResult result);
};

} // namespace nx::network::http::tunneling
