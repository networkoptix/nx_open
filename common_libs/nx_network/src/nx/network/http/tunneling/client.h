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

namespace nx_http::tunneling {

using TunnelValidatorFactoryFunc =
    std::function<std::unique_ptr<AbstractTunnelValidator>(
        std::unique_ptr<AbstractStreamSocket> /*tunnel*/,
        const Response&)>;

/**
 * Establishes connection to nx_http::tunneling::Server listening same request path.
 * For description of tunneling methods supported see nx_http::tunneling::Server.
 * The client can try to use different tunneling methods to find the one that works.
 */
class NX_NETWORK_API Client:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    /**
     * @param baseTunnelUrl Path must be equal to that passed to Server().
     */
    Client(
        const QUrl& baseTunnelUrl,
        const std::string& userTag = "");

    void setTunnelValidatorFactory(TunnelValidatorFactoryFunc func);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void setTimeout(std::chrono::milliseconds timeout);

    void setCustomHeaders(HttpHeaders headers);

    void openTunnel(OpenTunnelCompletionHandler completionHandler);

    const Response& response() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const QUrl m_baseTunnelUrl;
    std::unique_ptr<detail::BaseTunnelClient> m_actualClient;
    TunnelValidatorFactoryFunc m_validatorFactory;
    OpenTunnelCompletionHandler m_completionHandler;
    OpenTunnelResult m_lastResult;
    std::unique_ptr<AbstractTunnelValidator> m_validator;
    std::optional<std::chrono::milliseconds> m_timeout;

    void handleOpenTunnelCompletion(OpenTunnelResult result);
    void handleTunnelValidationResult(ResultCode result);
};

} // namespace nx_http::tunneling
