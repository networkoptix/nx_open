#pragma once

#include <nx/p2p/transport/i_p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/network/websocket/websocket.h>

namespace nx::p2p {

class P2PWebsocketTransport : public IP2PTransport
{
public:
    P2PWebsocketTransport(
        std::unique_ptr<network::AbstractStreamSocket> socket,
        network::websocket::FrameType frameType);

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        network::IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer& buffer,
        network::IoCompletionHandler handler) override;
    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual network::aio::AbstractAioThread* getAioThread() const override;
    virtual void pleaseStopSync() override;
    virtual network::SocketAddress getForeignAddress() const override;
    virtual void start(
        utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart = nullptr) override;

private:
    network::WebSocketPtr m_webSocket;
};

} // namespace nx::network
