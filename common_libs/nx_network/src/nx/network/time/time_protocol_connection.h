#pragma once

#include <chrono>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/stream_socket_server.h>

namespace nx {
namespace network {

class NX_NETWORK_API TimeProtocolConnection:
    public network::aio::BasicPollable
{
public:
    TimeProtocolConnection(
        network::server::StreamConnectionHolder<TimeProtocolConnection>* socketServer,
        std::unique_ptr<AbstractStreamSocket> _socket);

    void startReadingConnection(boost::optional<std::chrono::milliseconds> inactivityTimeout);

    std::chrono::milliseconds lifeDuration() const;
    int messagesReceivedCount() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    network::server::StreamConnectionHolder<TimeProtocolConnection>* m_socketServer;
    std::unique_ptr<AbstractStreamSocket> m_socket;
    nx::Buffer m_outputBuffer;
    std::chrono::steady_clock::time_point m_creationTimestamp;

    void onDataSent(
        SystemError::ErrorCode errorCode,
        size_t bytesSent);
};

} // namespace network
} // namespace nx
