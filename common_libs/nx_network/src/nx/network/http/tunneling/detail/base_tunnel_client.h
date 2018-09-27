#pragma once

#include <chrono>

#include <nx/network/aio/basic_pollable.h>

#include "../../http_async_client.h"
#include "../../http_types.h"

namespace nx_http::tunneling::detail {

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

class NX_NETWORK_API BaseTunnelClient:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    BaseTunnelClient(
        const QUrl& baseTunnelUrl,
        ClientFeedbackFunction clientFeedbackFunction);
    virtual ~BaseTunnelClient() = default;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void setTimeout(std::chrono::milliseconds timeout) = 0;

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) = 0;

    virtual const Response& response() const = 0;

protected:
    const QUrl m_baseTunnelUrl;
    std::unique_ptr<AsyncClient> m_httpClient;
    ClientFeedbackFunction m_clientFeedbackFunction;
    OpenTunnelCompletionHandler m_completionHandler;
    std::unique_ptr<AbstractStreamSocket> m_connection;

    virtual void stopWhileInAioThread() override;

    void cleanupFailedTunnel();
    void reportFailure(OpenTunnelResult result);

    bool resetConnectionAttributes();

    void reportSuccess();
};

} // namespace nx_http::tunneling::detail
