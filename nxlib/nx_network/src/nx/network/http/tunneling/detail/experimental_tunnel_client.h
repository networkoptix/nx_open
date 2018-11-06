#pragma once

#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/std/optional.h>

#include "base_tunnel_client.h"
#include "request_paths.h"
#include "../../http_async_client.h"
#include "../../http_types.h"

namespace nx::network::http::tunneling::detail {

class NX_NETWORK_API ExperimentalTunnelClient:
    public BaseTunnelClient
{
    using base_type = BaseTunnelClient;

public:
    ExperimentalTunnelClient(
        const nx::utils::Url& baseTunnelUrl,
        ClientFeedbackFunction clientFeedbackFunction);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void setTimeout(std::chrono::milliseconds timeout) override;

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) override;

    virtual const Response& response() const override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::string m_tunnelId;
    nx::utils::Url m_tunnelUrl;
    Response m_openTunnelResponse;
    std::optional<std::chrono::milliseconds> m_timeout;
    aio::Timer m_timer;
    std::unique_ptr<AsyncClient> m_downChannelHttpClient;
    std::unique_ptr<AsyncClient> m_upChannelHttpClient;

    std::unique_ptr<AbstractStreamSocket> m_downChannel;
    std::unique_ptr<AbstractStreamSocket> m_upChannel;

    void clear();

    void initiateDownChannel();
    void onDownChannelOpened();

    void initiateUpChannel();
    void onUpChannelOpened();

    void initiateChannel(
        AsyncClient* httpClient,
        http::Method::ValueType httpMethod,
        const std::string& requestPath,
        std::function<void()> requestHandler);

    void handleTunnelFailure(AsyncClient* failedHttpClient);
    void handleTunnelFailure(SystemError::ErrorCode systemErrorCode);

    void prepareOpenUpChannelRequest();

    void reportTunnelIfReady();
};

} // namespace nx::network::http::tunneling::detail
