// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/retry_timer.h>
#include <nx/network/stun/message.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

namespace nx {
namespace network {
namespace stun {

constexpr int kEveryIndicationMethod = 0;

class NX_NETWORK_API AbstractAsyncClient:
    public network::aio::BasicPollable
{
public:
    struct Settings
    {
        std::chrono::milliseconds sendTimeout;
        std::chrono::milliseconds recvTimeout;
        nx::network::RetryPolicy reconnectPolicy;

        Settings():
            sendTimeout(3000),
            recvTimeout(3000),
            reconnectPolicy(
                nx::network::RetryPolicy::kInfiniteRetries,
                std::chrono::milliseconds(500),
                nx::network::RetryPolicy::kDefaultDelayMultiplier,
                std::chrono::minutes(1),
                nx::network::RetryPolicy::kDefaultRandomRatio)
        {}
    };

    virtual ~AbstractAsyncClient() = default;

    using ConnectHandler = nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;
    using IndicationHandler = std::function<void(Message)>;
    using ReconnectHandler = std::function<void()>;
    using OnConnectionClosedHandler = nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;
    using RequestHandler = utils::MoveOnlyFunc<void(SystemError::ErrorCode, Message)>;
    using TimerHandler = std::function<void()>;

    /**
     * Asynchronously opens connection to the server.
     *
     * @param url stun::/hostname:port/
     * @param handler Called when 1st connection attempt has passed regardless of its success.
     * NOTE: Shall be called only once (to provide address). Reconnect will happen automatically.
     */
    virtual void connect(const nx::utils::Url& url, ConnectHandler handler = nullptr) = 0;

    /**
     * Subscribes for certain indications. If there is already a handler registered for the given
     * method, the handler is replaced by the new one.
     *
     * @param method Indication method of interest.
     *    Use kEveryIndicationMethod constant to install handler
     *    that will receive every unhandled indication.
     * @param handler Will be called for each indication message.
     * @param client Can be used to cancel subscription.
     */
    virtual void setIndicationHandler(
        int method, IndicationHandler handler, void* client = 0) = 0;

    /**
     * Subscribes for the event of successful reconnect.
     *
     * @param handler is called on every successfull reconnect.
     * @param client Can be used to cancel subscription.
     */
    virtual void addOnReconnectedHandler(
        ReconnectHandler handler, void* client = 0) = 0;

    virtual void setOnConnectionClosedHandler(
        OnConnectionClosedHandler onConnectionClosedHandler) = 0;

    /** Sends message asynchronously
     *
     * @param requestHandler Triggered after response has been received or error
     *     has occured. Message attribute is valid only if first attribute value
     *     is SystemError::noError.
     * @param client Can be used to cancel subscription.
     * @return false, if could not start asynchronous operation.
     *
     * NOTE: It is valid to call this method independent of openConnection
     *      (connection will be opened automatically).
     */
    virtual void sendRequest(
        Message request, RequestHandler handler, void* client = 0) = 0;

    /**
     * Schedules repetable timer until disconnect.
     * @return false if timer has not been scheduled (e.g., no connection is established at the moment).
     */
    virtual bool addConnectionTimer(
        std::chrono::milliseconds period, TimerHandler handler, void* client) = 0;

    /**
     * @return local address if the client is connected to the server.
     */
    virtual SocketAddress localAddress() const = 0;

    /**
     * @return server address if the client knows one.
     */
    virtual SocketAddress remoteAddress() const = 0;

    /**
     * Closes connection, also engages reconnect.
     */
    virtual void closeConnection(SystemError::ErrorCode errorCode) = 0;

    /**
     * Cancels all handlers, passed with client.
     */
    virtual void cancelHandlers(
        void* client, utils::MoveOnlyFunc<void()> handler) = 0;

    /**
     * If called in object's AIO thread then cancels immediately and without blocking.
     * If called from any other thread, then it can block.
     */
    virtual void cancelHandlersSync(void* client) = 0;

    /**
     * Configures connection keep-alive options.
     */
    virtual void setKeepAliveOptions(KeepAliveOptions options) = 0;
};

} // namespace stun
} // namespace network
} // namespace nx
