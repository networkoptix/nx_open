#pragma once

#include "i_p2p_transport_delegate.h"
#include <nx/network/websocket/websocket.h>

namespace nx::network::detail {

class P2TransportWebsocketDelegate: public IP2PSocketDelegate
{
public:
    P2TransportWebsocketDelegate(std::unique_ptr<AbstractStreamSocket> streamSocket,
        websocket::FrameType frameType = websocket::FrameType::binary);

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void pleaseStopSync() override;
    virtual SocketAddress getForeignAddress() const override;

private:
    WebSocketPtr m_webSocket;
};

} // namespace nx::network::detail