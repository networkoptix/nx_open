#pragma once

#include <list>
#include <memory>
#include <set>

#include <QtCore/QUrl>

#include <nx/network/http/http_async_client.h>
#include <nx/utils/thread/mutex.h>

#include "async_client.h"

namespace nx {
namespace stun {

/**
 * Establishes STUN over HTTP tunnel if http url is supplied.
 */
class NX_NETWORK_API AsyncClientWithHttpTunneling:
    public AbstractAsyncClient
{
    using base_type = AbstractAsyncClient;

public:
    AsyncClientWithHttpTunneling(Settings settings);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread);

    virtual void connect(
        SocketAddress endpoint,
        bool useSsl = false,
        ConnectHandler handler = nullptr) override;

    virtual bool setIndicationHandler(
        int method,
        IndicationHandler handler,
        void* client = nullptr) override;

    virtual void addOnReconnectedHandler(
        ReconnectHandler handler,
        void* client = nullptr) override;

    virtual void sendRequest(
        Message request,
        RequestHandler handler,
        void* client = nullptr) override;

    virtual bool addConnectionTimer(
        std::chrono::milliseconds period,
        TimerHandler handler,
        void* client) override;

    virtual SocketAddress localAddress() const override;

    virtual SocketAddress remoteAddress() const override;

    virtual void closeConnection(SystemError::ErrorCode errorCode) override;

    virtual void cancelHandlers(
        void* client,
        utils::MoveOnlyFunc<void()> handler) override;

    virtual void setKeepAliveOptions(KeepAliveOptions options) override;

    /**
     * @param url http and stun scheme is supported.
     */
    void connect(const QUrl& url, ConnectHandler handler);

private:
    Settings m_settings;
    std::unique_ptr<AsyncClient> m_stunClient;
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    ConnectHandler m_connectHandler;
    std::list<nx::utils::MoveOnlyFunc<void(AbstractAsyncClient*)>> m_cachedStunClientCalls;
    std::set<int> m_handledIndications;
    mutable QnMutex m_mutex;

    virtual void stopWhileInAioThread() override;

    void openHttpTunnel(const QUrl& url, ConnectHandler handler);
    void onHttpConnectionUpgradeDone();
    void makeCachedStunClientCalls();

    void invokeOrPostpone(nx::utils::MoveOnlyFunc<void(AbstractAsyncClient*)> func);
};

} // namespace stun
} // namespace nx
