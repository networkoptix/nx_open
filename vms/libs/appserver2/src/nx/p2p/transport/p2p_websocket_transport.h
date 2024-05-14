// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/websocket/websocket.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/p2p/transport/i_p2p_transport.h>

namespace nx::p2p {

class P2PWebsocketTransport : public IP2PTransport
{
public:
    P2PWebsocketTransport(
        std::unique_ptr<network::AbstractStreamSocket> socket,
        network::websocket::Role role,
        network::websocket::FrameType frameType,
        network::websocket::CompressionType compressionType,
        std::chrono::milliseconds aliveTimeout);

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        network::IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer* buffer,
        network::IoCompletionHandler handler) override;
    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual network::aio::AbstractAioThread* getAioThread() const override;
    virtual void pleaseStopSync() override;
    virtual network::SocketAddress getForeignAddress() const override;
    virtual void start(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart = nullptr) override;

    virtual QString lastErrorMessage() const override;
private:
    network::WebSocketPtr m_webSocket;
};

} // namespace nx::network
