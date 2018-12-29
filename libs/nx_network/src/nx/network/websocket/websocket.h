#pragma once

#include <memory>
#include <queue>
#include <QtCore>
#include <nx/utils/log/log.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/aio/timer.h>
#include <nx/network/buffer.h>
#include "websocket_parser.h"
#include "websocket_serializer.h"
#include "websocket_common_types.h"
#include "websocket_multibuffer.h"

namespace nx {
namespace network {
namespace websocket {

class NX_NETWORK_API WebSocket :
    public aio::AbstractAsyncChannel,
    private websocket::ParserHandler
{
public:
    /**
     * If ROLE is undefined, payload won't be masked (unmasked). FRAME_TYPE may be only BINARY or
     * TEXT, all other values are ignored and internal frame type is set to BINARY.
     */
    WebSocket(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        SendMode sendMode = SendMode::singleMessage,
        ReceiveMode receiveMode = ReceiveMode::message,
        Role role = Role::undefined,
        FrameType frameType= FrameType::binary);

    WebSocket(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        FrameType frameType);

    ~WebSocket();

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;

    /**
     * Cancels all IO operations. Socket is not operational after this function is called.
     */
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Should be called before first read/send. Typically after instantiation and bindToAioThread().
     * If you need to call it on working socket, first call cancelIOSync().
     */
    void start();

    /**
     * Makes sense only in multiFrameMessage mode.
     * Indicates that the next sendAsync will close current message
     */
    void setIsLastFrame();
    void sendCloseAsync(); /**< Send close frame */
    /**
     * After this timeout expired without any read activity read handler (if any) will be invoked
     * with SystemError::timedOut. Should be called before start().
     */
    void setAliveTimeout(std::chrono::milliseconds timeout);
    AbstractStreamSocket* socket() { return m_socket.get(); }
    const AbstractStreamSocket* socket() const { return m_socket.get(); }

private:
    using UserReadPair = std::pair<IoCompletionHandler, nx::Buffer* const>;
    using UserReadPairPtr = std::unique_ptr<UserReadPair>;

    std::unique_ptr<AbstractStreamSocket> m_socket;
    Parser m_parser;
    Serializer m_serializer;
    SendMode m_sendMode;
    ReceiveMode m_receiveMode;
    bool m_isLastFrame = false;
    bool m_isFirstFrame = true;
    std::queue<std::pair<IoCompletionHandler, nx::Buffer>> m_writeQueue;
    UserReadPairPtr m_userReadPair;
    websocket::MultiBuffer m_incomingMessageQueue;
    nx::Buffer m_controlBuffer;
    nx::Buffer m_readBuffer;
    std::unique_ptr<nx::network::aio::Timer> m_pingTimer;
    std::chrono::milliseconds m_aliveTimeout;
    nx::utils::ObjectDestructionFlag m_destructionFlag;
    bool m_failed = false;
    FrameType m_frameType;

    virtual void stopWhileInAioThread() override;

    /** Parser handler implementation */
    virtual void frameStarted(FrameType type, bool fin) override;
    virtual void framePayload(const char* data, int len) override;
    virtual void frameEnded() override;
    virtual void messageEnded() override;
    virtual void handleError(Error err) override;

    /** Own helper functions*/
    void processReadData();
    bool isDataFrame() const;
    void sendMessage(const nx::Buffer& message, IoCompletionHandler handler);
    void sendControlResponse(FrameType type);
    void sendControlRequest(FrameType type);
    void readWithoutAddingToQueueSync();
    void readWithoutAddingToQueue();
    void onPingTimer();
    void onRead(SystemError::ErrorCode ecode, size_t transferred);
    void onWrite(SystemError::ErrorCode ecode, size_t transferred);
    void handleSocketWrite(SystemError::ErrorCode ecode, size_t bytesSent);
    std::chrono::milliseconds pingTimeout() const;
    void callOnReadhandler(SystemError::ErrorCode error, size_t transferred);
    void callOnWriteHandler(SystemError::ErrorCode error, size_t transferred);
};

} // namespace websocket

using websocket::WebSocket;
using WebSocketPtr = std::unique_ptr<WebSocket>;

} // namespace network
} // namespace nx
