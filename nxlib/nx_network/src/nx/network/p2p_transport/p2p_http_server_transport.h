#pragma once

#include <nx/network/p2p_transport/i_p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/network/http/http_parser.h>

namespace nx::network {

class NX_NETWORK_API P2PHttpServerTransport: public IP2PTransport
{
public:
    P2PHttpServerTransport(std::unique_ptr<AbstractStreamSocket> socket,
        websocket::FrameType messageType = websocket::FrameType::binary);

    virtual ~P2PHttpServerTransport() override;

    void gotPostConnection(std::unique_ptr<AbstractStreamSocket> socket);

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void pleaseStopSync() override;
    virtual SocketAddress getForeignAddress() const override;

private:
    struct ReadContext
    {
        http::Message message;
        http::MessageParser parser;
        size_t bytesParsed = 0;
        nx::Buffer buffer;

        void reset();
    };

    std::unique_ptr<AbstractStreamSocket> m_sendSocket;
    std::unique_ptr<AbstractStreamSocket> m_readSocket;
    websocket::FrameType m_messageType;
    bool m_firstSend = true;
    ReadContext m_readContext;

    void onBytesRead(
        SystemError::ErrorCode error,
        size_t transferred,
        nx::Buffer* const buffer,
        IoCompletionHandler handler);

    void readFromSocket(nx::Buffer* const buffer, IoCompletionHandler handler);
};

} // namespace nx::network
