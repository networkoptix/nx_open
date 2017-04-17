#pragma once

#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/websocket/websocket_parser.h>
#include <nx/network/buffer.h>
#include <nx/network/websocket/websocket_serializer.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/websocket/websocket_common_types.h>

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
        singleMessage,
        multiFrameMessage
    };

    enum class ReceiveMode
    {
        frame,
        message,
        stream
    };

public:
    Websocket(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        const nx::Buffer& requestData,
        Role role = Role::undefined); /**< if role is undefined, payload won't be masked (unmasked) */

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    virtual void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

    void setSendMode(SendMode mode);
    SendMode sendMode() const;

    /** 
     * Makes sense only in multiFrameMessage mode. 
     * Indicates that the next sendAsync will close current message 
     */
    void setIsLastFrame();

    void setReceiveMode(ReceiveMode mode);
    ReceiveMode receiveMode() const;

    void setPayloadType(PayloadType type);
    PayloadType prevFramePayloadType() const;

    template<typename Rep, typename Period>
    void setKeepAliveTimeout(std::chrono::duration<Rep, Period> timeout)
    {
        m_keepAliveTimeout = timeout;
    }

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

private:
    std::chrono::milliseconds m_keepAliveTimeout;
    nx_api::BaseServerConnectionWrapper m_baseConnection;
    Parser m_parser;
    Serializer m_serializer;
    SendMode m_sendMode;
    ReceiveMode m_receiveMode;
    bool m_isLastFrame;
    bool m_isFirstFrame;
    PayloadType m_payloadType;
    PayloadType m_receivedPayloadType;
    std::function<void(SystemError::ErrorCode, size_t)> m_readHandler;
    std::function<void(SystemError::ErrorCode, size_t)> m_writeHandler;
    nx::Buffer* m_readBuffer;
    nx::Buffer m_buffer;
    nx::Buffer m_writeBuffer;
    nx::Buffer m_requestData;
};

} // namespace websocket
} // namespace network
} // namespace nx