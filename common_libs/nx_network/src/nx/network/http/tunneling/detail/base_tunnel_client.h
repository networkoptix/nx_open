#pragma once

#include <chrono>

#include <nx/network/aio/basic_pollable.h>

#include "../../http_async_client.h"
#include "../../http_types.h"

namespace nx_http::tunneling {

enum class ResultCode
{
    ok = 0,
    httpUpgradeFailed,
    ioError,
};

NX_NETWORK_API std::string toString(ResultCode);

struct NX_NETWORK_API OpenTunnelResult
{
    ResultCode resultCode = ResultCode::ok;
    std::unique_ptr<AbstractStreamSocket> connection;
    /** Defined only if !resultCode. */
    SystemError::ErrorCode sysError = SystemError::noError;
    /** Defined only if !resultCode && !sysError. */
    StatusCode::Value httpStatus = StatusCode::ok;

    OpenTunnelResult() = default;

    /** Constructor of a successful result. */
    OpenTunnelResult(std::unique_ptr<AbstractStreamSocket> connection);

    /** Constructor for a I/O error. resultCode is set to ResultCode>::ioError. */
    OpenTunnelResult(SystemError::ErrorCode sysError);

    std::string toString() const;
};

using OpenTunnelCompletionHandler =
    nx::utils::MoveOnlyFunc<void(OpenTunnelResult)>;

namespace detail {

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

    ClientFeedbackFunction takeFeedbackFunction();
    void setCustomHeaders(HttpHeaders headers);
    const HttpHeaders& customHeaders() const;

protected:
    const QUrl m_baseTunnelUrl;
    std::unique_ptr<AsyncClient> m_httpClient;
    OpenTunnelCompletionHandler m_completionHandler;
    std::unique_ptr<AbstractStreamSocket> m_connection;

    virtual void stopWhileInAioThread() override;

    void cleanUpFailedTunnel();
    void cleanUpFailedTunnel(AsyncClient* httpClient);
    void cleanUpFailedTunnel(SystemError::ErrorCode systemErrorCode);

    void reportFailure(OpenTunnelResult result);

    bool resetConnectionAttributes();

    void reportSuccess();

private:
    ClientFeedbackFunction m_clientFeedbackFunction;
    HttpHeaders m_customHeaders;
};

} // namespace detail

} // namespace nx_http::tunneling
