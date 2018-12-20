#pragma once

#include "base_tunnel_client.h"
#include "request_paths.h"

namespace nx::network::http::tunneling::detail {

class NX_NETWORK_API MultiMessageClient:
    public BaseTunnelClient
{
    using base_type = BaseTunnelClient;

public:
    MultiMessageClient(
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
    Response m_response;
};

} // namespace nx::network::http::tunneling::detail
