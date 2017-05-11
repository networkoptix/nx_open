#pragma once

#include <memory>
#include <QtCore>
#include <nx/utils/log/log.h>
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
    public QObject,
    public aio::AbstractAsyncChannel,
    private nx::network::server::BaseServerConnectionHandler,
    private websocket::ParserHandler,
    private nx::network::server::StreamConnectionHolder<nx::network::server::BaseServerConnectionWrapper>
{
    friend struct BaseServerConnectionAccess;

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

    Q_OBJECT

public:

    WebSocket(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        SendMode sendMode = SendMode::singleMessage,
        ReceiveMode receiveMode = ReceiveMode::message,
        Role role = Role::undefined); /**< if role is undefined, payload won't be masked (unmasked) */

    virtual void readSomeAsync(nx::Buffer* const buffer, HandlerType handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, HandlerType handler) override;
    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Makes sense only in multiFrameMessage mode.
     * Indicates that the next sendAsync will close current message
     */
    void setIsLastFrame();
    void sendCloseAsync(); /**< Send close frame */
    void setPingTimeout(std::chrono::milliseconds timeout);

signals:
    /**
     * Connection is not usable after this signal is emitted.
     */
    void connectionClosed();
    void pingReceived();
    void pongReceived();


private:
    /**  BaseServerConnectionHandler implementation */
    virtual void bytesReceived(nx::Buffer& buffer) override;
    virtual void readyToSendData() override;

    /** StreamConnectionHolder implementation */
    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection) override;


    virtual void stopWhileInAioThread() override;

    /** Parser handler implementation */
    virtual void frameStarted(FrameType type, bool fin) override;
    virtual void framePayload(const char* data, int len) override;
    virtual void frameEnded() override;
    virtual void messageEnded() override;
    virtual void handleError(Error err) override;

    /** Own helper functions*/
    void handleRead();
    bool isDataFrame() const;
    void sendPreparedMessage(nx::Buffer* buffer, int writeSize, HandlerType handler);
    void sendControlResponse(FrameType requestType, FrameType responseType);
    void sendControlRequest(FrameType type);
    void readWithoutAddingToQueue();
    void handlePingTimer();

private:
    std::unique_ptr<nx::network::server::BaseServerConnectionWrapper> m_baseConnection;
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
    std::unique_ptr<nx::network::aio::Timer> m_pingTimer;
    bool m_terminating;
    std::chrono::milliseconds m_pingTimeout;
};

} // namespace websocket

using websocket::WebSocket;
using WebSocketPtr = std::shared_ptr<WebSocket>;

} // namespace network
} // namespace nx
