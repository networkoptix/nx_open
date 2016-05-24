#pragma once

#include <nx/network/stun/abstract_async_client.h>

namespace nx {
namespace stun {

/** AsyncClient wrapper.
 * Can be stopped (to prevent async calls) while AsyncClient still running */
class NX_NETWORK_API AsyncClientUser
:
    public QnStoppableAsync
{
public:
    AsyncClientUser(const AsyncClientUser&) = delete;
    AsyncClientUser(AsyncClientUser&&) = delete;
    AsyncClientUser& operator=(const AsyncClientUser&) = delete;
    AsyncClientUser& operator=(AsyncClientUser&&) = delete;

    /** Returns local connection address in case if client is connected to STUN server */
    SocketAddress localAddress() const;

    /** Returns STUN server address in case if client has one */
    SocketAddress remoteAddress() const;

    /** Handler called just after broken tcp connection has been restored */
    void setOnReconnectedHandler(AbstractAsyncClient::ReconnectHandler handler);

    /** Shall be called before the last shared_pointer is gone */
    void pleaseStop(utils::MoveOnlyFunc<void()> handler) override;

protected:
    AsyncClientUser(std::shared_ptr<AbstractAsyncClient> client);

    void sendRequest(Message request, AbstractAsyncClient::RequestHandler handler);
    bool setIndicationHandler(int method, AbstractAsyncClient::IndicationHandler handler);

private:
    std::shared_ptr<AbstractAsyncClient> m_client;
};

} // namespase stun
} // namespase nx
