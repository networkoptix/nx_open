#ifndef NX_STUN_ASYNC_CLIENT_USER_H
#define NX_STUN_ASYNC_CLIENT_USER_H

#include "async_client.h"

namespace nx {
namespace stun {

/** Shared \class AsyncClient usadge wrapper */
class NX_NETWORK_API AsyncClientUser
    : public std::enable_shared_from_this<AsyncClientUser>
    , public QnStoppableAsync
{
public:
    ~AsyncClientUser();

    /** Returns local connection address in case if client is connected to STUN server */
    SocketAddress localAddress() const;

    /** Returns STUN server address in case if client has one */
    SocketAddress remoteAddress() const;

    /** Shall be called before the last shared_pointer is gone */
    virtual void pleaseStop(std::function<void()> handler) override;

protected:
    AsyncClientUser(std::shared_ptr<AbstractAsyncClient> client);

    void sendRequest(Message request, AbstractAsyncClient::RequestHandler handler);
    bool setIndicationHandler(int method, AbstractAsyncClient::IndicationHandler handler);

private:
    bool startOperation();
    void stopOperation();
    void checkHandler(QnMutexLockerBase* lk);

    QnMutex m_mutex;
    size_t m_operationsInProgress;
    std::shared_ptr<AbstractAsyncClient> m_client;
    std::function<void()> m_stopHandler;
};

} // namespase stun
} // namespase nx

#endif // ASYNC_CLIENT_USER_H
