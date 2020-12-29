#pragma once

#include <nx/p2p/transport/i_p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/network/http/http_parser.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/object_destruction_flag.h>

namespace nx::p2p {

class P2PHttpServerTransport: public IP2PTransport
{
public:
    P2PHttpServerTransport(
        std::unique_ptr<network::AbstractStreamSocket> socket,
        network::websocket::FrameType messageType = network::websocket::FrameType::binary);

    virtual ~P2PHttpServerTransport() override;

    /**
     * Provide a socket for a POST incoming connection with this function. You can optionally
     * provide a body of already accepted and read POST request.
     */
    void gotPostConnection(
        std::unique_ptr<network::AbstractStreamSocket> socket,
        const nx::Buffer& body = nx::Buffer());

    /**
     * This should be called before all other operations. Transport becomes operational only after
     * onGetRequestReceived() callback is fired.
     */
    virtual void start(
        utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onGetRequestReceived) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer, 
        network::IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer& buffer, 
        network::IoCompletionHandler handler) override;
    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual network::aio::AbstractAioThread* getAioThread() const override;
    virtual network::SocketAddress getForeignAddress() const override;

private:
    struct ReadContext
    {
        network::http::Message message;
        network::http::MessageParser parser;
        size_t bytesParsed = 0;
        nx::Buffer buffer;

        void reset();
    };

    using UserReadHandlerPair = 
        std::unique_ptr<std::pair<nx::Buffer* const, network::IoCompletionHandler>>;

    std::unique_ptr<network::AbstractStreamSocket> m_sendSocket;
    std::unique_ptr<network::AbstractStreamSocket> m_readSocket;
    network::websocket::FrameType m_messageType;
    nx::Buffer m_sendBuffer;
    nx::Buffer m_responseBuffer;
    nx::Buffer m_providedPostBody;
    nx::Buffer m_sendChannelReadBuffer;
    bool m_firstSend = true;
    ReadContext m_readContext;
    network::aio::Timer m_timer;
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onGetRequestReceived =
        [](SystemError::ErrorCode) {};
    bool m_failed = false;
    utils::ObjectDestructionFlag m_destructionFlag;
    UserReadHandlerPair m_userReadHandlerPair;

    void onBytesRead(
        SystemError::ErrorCode error,
        size_t transferred,
        nx::Buffer* const buffer,
        network::IoCompletionHandler handler);

    void readFromSocket(nx::Buffer* const buffer, network::IoCompletionHandler handler);
    QByteArray makeInitialResponse() const;
    QByteArray makeFrameHeader() const;
    void sendPostResponse(
        SystemError::ErrorCode error,
        network::IoCompletionHandler userHandler,
        utils::MoveOnlyFunc<void(SystemError::ErrorCode, network::IoCompletionHandler)> completionHandler);
    void onReadFromSendSocket(SystemError::ErrorCode error, size_t transferred);

    static void addDateHeader(network::http::HttpHeaders* headers);

    virtual void stopWhileInAioThread() override;
};

} // namespace nx::network
