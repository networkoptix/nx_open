#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/retry_timer.h>
#include <nx/network/stun/message.h>
#include <nx/utils/move_only_func.h>

namespace nx {
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
                std::chrono::minutes(1))
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
     * @param handler Is called when 1st connection attempt has passed regadless of it's success.
     * NOTE: shall be called only once (to provide address) reconnect will
     *      happen automatically.
     */
    virtual void connect(const QUrl& url, ConnectHandler handler = nullptr) = 0;

    /**
     * Subscribes for certain indications.
     *
     * @param method Indication method of interest.
     *    Use kEveryIndicationMethod constant to install handler that will received all unhandled indications.
     * @param handler Will be called for each indication message.
     * @param client Can be used to cancel subscription.
     * @return true on success, false if this methed is already monitored.
     */
    virtual bool setIndicationHandler(
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
     *      (connection will be opened automaticaly).
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
     * @return local address if client is connected to the server.
     */
    virtual SocketAddress localAddress() const = 0;

    /**
     * @return server address if client knows one.
     */
    virtual SocketAddress remoteAddress() const = 0;

    /**
     * Closes connection, also engage reconnect.
     */
    virtual void closeConnection(SystemError::ErrorCode errorCode) = 0;

    /**
     * Cancels all handlers, passed with @param client.
     */
    virtual void cancelHandlers(
        void* client, utils::MoveOnlyFunc<void()> handler) = 0;

    /**
     * Configures connection keep alive options.
     */
    virtual void setKeepAliveOptions(KeepAliveOptions options) = 0;
};

} // namespace stun
} // namespace nx
