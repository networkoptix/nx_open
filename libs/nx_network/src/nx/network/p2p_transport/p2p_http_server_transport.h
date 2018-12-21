#pragma once

#include <nx/network/p2p_transport/i_p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/network/http/http_parser.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/object_destruction_flag.h>

namespace nx::network {

class NX_NETWORK_API P2PHttpServerTransport: public IP2PTransport
{
public:
    P2PHttpServerTransport(
        std::unique_ptr<AbstractStreamSocket> socket,
        websocket::FrameType messageType = websocket::FrameType::binary);

    virtual ~P2PHttpServerTransport() override;

    /**
     * Provide a socket for a POST incoming connection with this function. You can optionally
     * provide a body of already accepted and read POST request.
     */
    void gotPostConnection(
        std::unique_ptr<AbstractStreamSocket> socket,
        const nx::Buffer& body = nx::Buffer());

    /**
     * This should be called before all other operations. Transport becomes operational only after
     * onGetRequestReceived() callback is fired.
     */
    virtual void start(
        utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onGetRequestReceived) override;

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual aio::AbstractAioThread* getAioThread() const override;
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

    using UserReadHandlerPair = std::unique_ptr<std::pair<nx::Buffer* const, IoCompletionHandler>>;

    std::unique_ptr<AbstractStreamSocket> m_sendSocket;
    std::unique_ptr<AbstractStreamSocket> m_readSocket;
    websocket::FrameType m_messageType;
    nx::Buffer m_sendBuffer;
    nx::Buffer m_responseBuffer;
    nx::Buffer m_providedPostBody;
    nx::Buffer m_sendChannelReadBuffer;
    bool m_firstSend = true;
    ReadContext m_readContext;
    aio::Timer m_timer;
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onGetRequestReceived =
        [](SystemError::ErrorCode) {};
    bool m_failed = false;
    utils::ObjectDestructionFlag m_destructionFlag;
    UserReadHandlerPair m_userReadHandlerPair;

    void onBytesRead(
        SystemError::ErrorCode error,
        size_t transferred,
        nx::Buffer* const buffer,
        IoCompletionHandler handler);

    void readFromSocket(nx::Buffer* const buffer, IoCompletionHandler handler);
    QByteArray makeInitialResponse() const;
    QByteArray makeFrameHeader() const;
    void sendPostResponse(
        SystemError::ErrorCode error,
        IoCompletionHandler userHandler,
        utils::MoveOnlyFunc<void(SystemError::ErrorCode, IoCompletionHandler)> completionHandler);
    void onReadFromSendSocket(SystemError::ErrorCode error, size_t transferred);

    static void addDateHeader(http::HttpHeaders* headers);

    virtual void stopWhileInAioThread() override;
};

} // namespace nx::network
