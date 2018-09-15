#pragma once

#include <nx/network/aio/basic_pollable.h>

#include "../../http_async_client.h"
#include "../../http_types.h"

namespace nx::network::http::tunneling::detail {

struct OpenTunnelResult
{
    SystemError::ErrorCode sysError = SystemError::noError;
    StatusCode::Value httpStatus = StatusCode::ok;
    std::unique_ptr<AbstractStreamSocket> connection;
};

using OpenTunnelCompletionHandler =
    nx::utils::MoveOnlyFunc<void(OpenTunnelResult)>;

using ClientFeedbackFunction = nx::utils::MoveOnlyFunc<void(bool /*success*/)>;

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API BasicTunnelClient:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    BasicTunnelClient(
        const nx::utils::Url& baseTunnelUrl,
        ClientFeedbackFunction clientFeedbackFunction);
    virtual ~BasicTunnelClient() = default;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) = 0;

    virtual const Response& response() const = 0;

protected:
    const nx::utils::Url m_baseTunnelUrl;
    std::unique_ptr<AsyncClient> m_httpClient;
    ClientFeedbackFunction m_clientFeedbackFunction;
    OpenTunnelCompletionHandler m_completionHandler;
    std::unique_ptr<network::AbstractStreamSocket> m_connection;

    virtual void stopWhileInAioThread() override;

    void cleanupFailedTunnel();
    void reportFailure(OpenTunnelResult result);

    bool resetConnectionAttributes();

    void reportSuccess();
};

} // namespace nx::network::http::tunneling::detail
