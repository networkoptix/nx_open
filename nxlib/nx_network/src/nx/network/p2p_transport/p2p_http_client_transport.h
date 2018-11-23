#pragma once

#include <nx/network/p2p_transport/i_p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/network/http/http_async_client.h>

namespace nx::network {

class NX_NETWORK_API P2PHttpClientTransport: public IP2PTransport
{
private:
    using HttpClientPtr = std::unique_ptr<nx::network::http::AsyncClient>;

public:
    P2PHttpClientTransport(
        HttpClientPtr readHttpClient,
        nx::utils::MoveOnlyFunc<void()> onPostConnectionEstablished,
        websocket::FrameType frameType = websocket::FrameType::binary);

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void pleaseStopSync() override;
    virtual SocketAddress getForeignAddress() const override;

private:
    HttpClientPtr m_writeHttpClient;
    HttpClientPtr m_readHttpClient;
    websocket::FrameType m_messageType;
};

} // namespace nx::network
