#pragma once

#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/websocket/parser.h>
#include <nx/network/websocket/serializer.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/websocket/common_types.h>

namespace nx {
namespace network {
namespace websocket {

class AsyncChannel : 
    public aio::AbstractAsyncChannel,
    private nx_api::BaseServerConnection<AsyncChannel>
{
    friend struct BaseServerConnectionAccess;
public:
    enum class SendMode
    {
        singleMessage,
        multiFrameMessage,
        multiFrameMessageLastFrame
    };

    enum class ReceiveMode
    {
        frame,
        message,
        stream
    };

public:
    AsyncChannel(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        const nx::Buffer& requestData,
        Role role = Role::undefined); //< if role is undefined, payload won't be masked (unmasked)

    void setSendMode(SendMode mode);
    SendMode sendMode() const;

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
    void on_bytesReceived(const nx::Buffer& buf);
    void on_readyToSendData();

private:
    std::chrono::milliseconds m_keepAliveTimeout;
};

}
}
}