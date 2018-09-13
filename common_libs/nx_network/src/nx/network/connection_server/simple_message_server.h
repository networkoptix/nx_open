#pragma once

#include <chrono>
#include <queue>

#include "stream_socket_server.h"

namespace nx {
namespace network {
namespace server {

class NX_NETWORK_API SimpleMessageServerConnection:
    public network::aio::BasicPollable
{
public:
    SimpleMessageServerConnection(
        StreamConnectionHolder<SimpleMessageServerConnection>* socketServer,
        std::unique_ptr<AbstractStreamSocket> _socket,
        nx::Buffer request,
        nx::Buffer response);

    void startReadingConnection(
        std::optional<std::chrono::milliseconds> /*inactivityTimeout*/);

    void setKeepConnection(bool val);

    std::chrono::milliseconds lifeDuration() const;
    int messagesReceivedCount() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    network::server::StreamConnectionHolder<SimpleMessageServerConnection>* m_socketServer;
    std::unique_ptr<AbstractStreamSocket> m_socket;
    nx::Buffer m_request;
    nx::Buffer m_response;
    nx::Buffer m_readBuffer;
    const std::chrono::steady_clock::time_point m_creationTimestamp;
    bool m_keepConnection = false;
    std::queue<nx::Buffer> m_sendQueue;

    void onDataRead(
        SystemError::ErrorCode errorCode,
        size_t bytesRead);

    void scheduleMessageSend();

    void sendNextMessage();

    void onDataSent(
        SystemError::ErrorCode errorCode,
        size_t bytesSent);
};

//-------------------------------------------------------------------------------------------------

/**
 * Sends predefined response after receiving expected request via every connection accepted.
 * If no request is defined, than response is sent immediately after accepting connection.
 */
class SimpleMessageServer:
    public server::StreamSocketServer<SimpleMessageServer, SimpleMessageServerConnection>
{
    using base_type =
        server::StreamSocketServer<SimpleMessageServer, SimpleMessageServerConnection>;

public:
    template<typename... Args>
    SimpleMessageServer(Args... args):
        base_type(std::move(args)...)
    {
    }

    void setRequest(nx::Buffer message)
    {
        m_request.swap(message);
    }

    void setResponse(nx::Buffer message)
    {
        m_response.swap(message);
    }

    void setKeepConnection(bool val)
    {
        m_keepConnection = val;
    }

protected:
    virtual std::shared_ptr<SimpleMessageServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        auto connection = std::make_shared<SimpleMessageServerConnection>(
            this,
            std::move(_socket),
            m_request,
            m_response);
        connection->setKeepConnection(m_keepConnection);
        return connection;
    }

private:
    nx::Buffer m_request;
    nx::Buffer m_response;
    bool m_keepConnection = false;
};

} // namespace server
} // namespace network
} // namespace nx
