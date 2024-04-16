// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <queue>

#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/aio/timer.h>
#include <nx/network/connection_server/base_server_connection.h>
#include <nx/utils/buffer.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/log/log.h>

#include "websocket_common_types.h"
#include "websocket_multibuffer.h"
#include "websocket_parser.h"
#include "websocket_serializer.h"

namespace nx::network {
namespace websocket {

class NX_NETWORK_API WebSocket : public aio::AbstractAsyncChannel
{
public:
    /**
     * If ROLE is undefined, payload won't be masked (unmasked). FRAME_TYPE may be only BINARY or
     * TEXT, all other values are ignored and internal frame type is set to BINARY.
     */
    WebSocket(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        SendMode sendMode,
        ReceiveMode receiveMode,
        Role role,
        FrameType frameType,
        network::websocket::CompressionType compressionType);

    WebSocket(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        Role role,
        FrameType frameType,
        network::websocket::CompressionType compressionType);

    ~WebSocket();

    /**
     * Wraps read/send with a Websocket message type.
     */
    void readAsync(Frame* const frame, IoCompletionHandler handler);
    void sendAsync(Frame&& frame, IoCompletionHandler handler);

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer* buffer, IoCompletionHandler handler) override;

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
    QString lastErrorMessage() const;

    /**
     * Disable PONG responses.
     * NOTE: Don't call it unless you are completely sure what you are doing.
     * NOTE: Should be called before start().
     */
    void disablePingPong();

private:
    struct UserReadContext
    {
        IoCompletionHandler handler;
        nx::Buffer* const bufferPtr;
        Frame* const framePtr;

        UserReadContext(IoCompletionHandler handler, nx::Buffer* const bufferPtr, Frame* const framePtr):
            handler(std::move(handler)),
            bufferPtr(bufferPtr),
            framePtr(framePtr)
        {}
    };
    using UserReadContextPtr = std::unique_ptr<UserReadContext>;

    std::unique_ptr<AbstractStreamSocket> m_socket;
    Parser m_parser;
    Serializer m_serializer;
    SendMode m_sendMode;
    ReceiveMode m_receiveMode;
    bool m_isLastFrame = false;
    bool m_isFirstFrame = true;
    std::queue<std::pair<IoCompletionHandler, nx::Buffer>> m_writeQueue;
    UserReadContextPtr m_userReadContext;
    websocket::MultiBuffer m_incomingMessageQueue;
    nx::Buffer m_controlBuffer;
    nx::Buffer m_readBuffer;
    std::unique_ptr<nx::network::aio::Timer> m_pingTimer;
    std::unique_ptr<nx::network::aio::Timer> m_pongTimer;
    std::chrono::milliseconds m_aliveTimeout;
    nx::utils::InterruptionFlag m_destructionFlag;
    bool m_failed = false;
    FrameType m_defaultFrameType;
    network::websocket::CompressionType m_compressionType;
    bool m_readingCeased = false;
    bool m_pingPongDisabled = false;
    QString m_lastErrorMessage;

    virtual void stopWhileInAioThread() override;

    /** Parser handler implementation */
    void gotFrame(FrameType type, nx::Buffer&& data, bool fin);

    /** Own helper functions*/
    void sendMessage(const nx::Buffer& message, int writeSize, IoCompletionHandler handler);
    void sendControlResponse(FrameType type);
    void sendControlRequest(FrameType type);
    void onPingTimer();
    void onRead(SystemError::ErrorCode ecode, size_t transferred);
    void onWrite(SystemError::ErrorCode ecode, size_t transferred);
    void readAnyAsync(
        nx::Buffer* const buffer, Frame* const frame, IoCompletionHandler handler);
    size_t handleRead(nx::Buffer* const bufferPtr, Frame* const framePtr);
    void callOnReadhandler(SystemError::ErrorCode error, size_t transferred);
    void callOnWriteHandler(SystemError::ErrorCode error, size_t transferred);
    void setFailedState(const QString& message);
};

} // namespace websocket

using websocket::WebSocket;
using WebSocketPtr = std::unique_ptr<WebSocket>;

} // namespace nx::network
