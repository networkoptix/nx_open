#pragma once

#include <nx/network/stun/abstract_async_client.h>
#include <nx/utils/async_operation_guard.h>

namespace nx {
namespace stun {

/**
 * AsyncClient wrapper which lives in a designated AIO thread.
 * Used to share single AbstractAsyncClient instance between multiple users.
 * Can be stopped (to prevent async calls) while AsyncClient still running.
 */
class NX_NETWORK_API AsyncClientUser:
    public network::aio::Timer
{
public:
    AsyncClientUser(std::shared_ptr<AbstractAsyncClient> client);
    virtual ~AsyncClientUser() override;

    AsyncClientUser(const AsyncClientUser&) = delete;
    AsyncClientUser(AsyncClientUser&&) = delete;
    AsyncClientUser& operator=(const AsyncClientUser&) = delete;
    AsyncClientUser& operator=(AsyncClientUser&&) = delete;

    /** Returns local connection address in case if client is connected to STUN server */
    SocketAddress localAddress() const;

    /** Returns STUN server address in case if client has one */
    SocketAddress remoteAddress() const;

    void sendRequest(Message request, AbstractAsyncClient::RequestHandler handler);
    bool setIndicationHandler(int method, AbstractAsyncClient::IndicationHandler handler);
    /** Handler called just after broken tcp connection has been restored */
    void setOnReconnectedHandler(AbstractAsyncClient::ReconnectHandler handler);

    /** Timer is called over and over again until connection is closed */
    bool setConnectionTimer(
        std::chrono::milliseconds period, AbstractAsyncClient::TimerHandler handler);

    /** Shall be called before the last shared_pointer is gone */
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync(bool checkForLocks = true) override;

    /** Return AbstractAsyncClient for configuration purposes */
    AbstractAsyncClient* client() const;

protected:
    void disconnectFromClient();

    utils::AsyncOperationGuard m_asyncGuard;
    std::shared_ptr<AbstractAsyncClient> m_client;
    AbstractAsyncClient::ReconnectHandler m_reconnectHandler;

    void reportReconnect();
};

} // namespace stun
} // namespace nx
