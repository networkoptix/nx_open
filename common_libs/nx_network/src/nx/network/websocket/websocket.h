#pragma once

#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/buffer.h>
#include "websocket_parser.h"
#include "websocket_serializer.h"
#include "websocket_common_types.h"
#include "websocket_multibuffer.h"

namespace nx {
namespace network {
namespace websocket {

class Websocket : 
    public aio::AbstractAsyncChannel,
    private nx_api::BaseServerConnectionHandler,
    private ParserHandler,
    private StreamConnectionHolder<nx_api::BaseServerConnectionWrapper>
{
    friend struct BaseServerConnectionAccess;
public:
    enum class SendMode
    {
        /** Wrap buffer passed to sendAsync() in a complete websocket message */
        singleMessage,  

        /** 
         * Wrap buffer passed to sendAsync() in a complete websocket message.
         * @note Call setIsLastFrame() to mark final frame in the message.
         */
        multiFrameMessage   
    };

    enum class ReceiveMode
    {
        frame,      /**< Read handler will be called only when complete frame has been read from socket */
        message,    /**< Read handler will be called only when complete message has been read from socket*/ 
        stream      /**< Read handler will be called only when any data has been read from socket */ 
    };

public:
    Websocket(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        const nx::Buffer& requestData,
        SendMode sendMode = SendMode::singleMessage,
        ReceiveMode receiveMode = ReceiveMode::message,
        Role role = Role::undefined); /**< if role is undefined, payload won't be masked (unmasked) */

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    virtual void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

    /** 
     * Makes sense only in multiFrameMessage mode. 
     * Indicates that the next sendAsync will close current message 
     */
    void setIsLastFrame();

    // TODO: implement
    void sendPingAsync(); /**< Send ping request */
    // TODO: implement
    void closeAsync(); /**< Send close frame */

private:
    /**  BaseServerConnectionHandler implementation */
    virtual void bytesReceived(nx::Buffer& buffer) override;
    virtual void readyToSendData(size_t count) override;

    /** StreamConnectionHolder implementation */
    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection ) override;


    /** Parser handler implementation */
    virtual void frameStarted(FrameType type, bool fin) override;
    virtual void framePayload(const char* data, int len) override;
    virtual void frameEnded() override;
    virtual void messageEnded() override;
    virtual void handleError(Error err) override;

    /** Own helper functions*/
    void handleRead();

private:
    nx_api::BaseServerConnectionWrapper m_baseConnection;
    Parser m_parser;
    Serializer m_serializer;
    SendMode m_sendMode;
    ReceiveMode m_receiveMode;
    bool m_isLastFrame;
    bool m_isFirstFrame;
    std::function<void(SystemError::ErrorCode, size_t)> m_readHandler;
    std::function<void(SystemError::ErrorCode, size_t)> m_writeHandler;
    nx::Buffer* m_readBuffer;
    nx::Buffer m_writeBuffer;
    MultiBuffer m_buffer;
};

} // namespace websocket
} // namespace network
} // namespace nx