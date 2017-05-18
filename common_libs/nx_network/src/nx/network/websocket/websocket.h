#pragma once

#include <memory>
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
#include "websocket_handler_queue.h"

namespace nx {
namespace network {
namespace websocket {

class NX_NETWORK_API WebSocket :
    public aio::AbstractAsyncChannel,
    private websocket::ParserHandler
{
    struct WriteData
    {
        nx::Buffer buffer;
        int writeSize;

        WriteData(nx::Buffer&& buffer, int writeSize):
            buffer(std::move(buffer)),
            writeSize(writeSize)
        {}

        WriteData(WriteData&& other) = default;
        WriteData& operator=(WriteData&& other) = default;

        WriteData() {}
    };

public:

    WebSocket(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        SendMode sendMode = SendMode::singleMessage,
        ReceiveMode receiveMode = ReceiveMode::message,
        Role role = Role::undefined); /**< if role is undefined, payload won't be masked (unmasked) */

    ~WebSocket();

    virtual void readSomeAsync(nx::Buffer* const buffer, HandlerType handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, HandlerType handler) override;
    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void stopWhileInAioThread() override;

    /**
     * Makes sense only in multiFrameMessage mode.
     * Indicates that the next sendAsync will close current message
     */
    void setIsLastFrame();
    void sendCloseAsync(); /**< Send close frame */
    void setPingTimeout(std::chrono::milliseconds timeout);
    AbstractStreamSocket* socket() { return m_socket.get(); }

protected:
    int m_pingsReceived = 0;
    int m_pongsReceived = 0;

private:
    /** Parser handler implementation */
    virtual void frameStarted(FrameType type, bool fin) override;
    virtual void framePayload(const char* data, int len) override;
    virtual void frameEnded() override;
    virtual void messageEnded() override;
    virtual void handleError(Error err) override;

    /** Own helper functions*/
    void processReadData(SystemError::ErrorCode ecode, size_t bytesRead);
    bool isDataFrame() const;
    void sendPreparedMessage(nx::Buffer* buffer, int writeSize, HandlerType handler);
    void sendControlResponse(FrameType requestType, FrameType responseType);
    void sendControlRequest(FrameType type);
    void readWithoutAddingToQueue();
    void readWithoutAddingToQueueAsync();
    void handlePingTimer();
    void handleSocketRead(SystemError::ErrorCode ecode, size_t bytesRead);
    void handleSocketWrite(SystemError::ErrorCode ecode, size_t bytesSent);

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    Parser m_parser;
    Serializer m_serializer;
    SendMode m_sendMode;
    ReceiveMode m_receiveMode;
    bool m_isLastFrame;
    bool m_isFirstFrame;
    HandlerQueue<WriteData> m_writeQueue;
    HandlerQueue<nx::Buffer*> m_readQueue;
    websocket::MultiBuffer m_userDataBuffer;
    nx::Buffer m_controlBuffer;
    nx::Buffer m_readBuffer;
    std::unique_ptr<nx::network::aio::Timer> m_pingTimer;
    std::chrono::milliseconds m_pingTimeout;
    nx::utils::ObjectDestructionFlag m_destructionFlag;
    Error m_lastError;
};

} // namespace websocket

using websocket::WebSocket;
using WebSocketPtr = std::unique_ptr<WebSocket>;

} // namespace network
} // namespace nx
