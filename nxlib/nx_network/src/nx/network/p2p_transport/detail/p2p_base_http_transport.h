#pragma once

#include "../i_p2p_transport.h"
#include <memory>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/network/http/http_async_client.h>

namespace nx::network::detail {

class NX_NETWORK_API P2PBaseHttpTransport: public IP2PTransport
{
public:
    P2PBaseHttpTransport(websocket::FrameType messageType = websocket::FrameType::binary);

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void pleaseStopSync() override;
    virtual SocketAddress getForeignAddress() const override;

protected:
    using HttpClientPtr = std::unique_ptr<nx::network::http::AsyncClient>;

    HttpClientPtr m_writeHttpClient;
    HttpClientPtr m_readHttpClient;
    websocket::FrameType m_messageType;
};

} // namespace nx::network::detail