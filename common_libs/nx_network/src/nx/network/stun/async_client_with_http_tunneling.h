#pragma once

#include <QtCore/QUrl>

#include "async_client.h"

namespace nx {
namespace stun {

class NX_NETWORK_API AsyncClientWithHttpTunneling:
    public AbstractAsyncClient
{
public:
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

    void connect(QUrl url, ConnectHandler handler);

private:
    AsyncClient m_stunClient;
};

} // namespase stun
} // namespase nx
