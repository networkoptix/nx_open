#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/stream_socket_server.h>

namespace nx {
namespace time_server {

class TimeProtocolConnection:
    public network::aio::BasicPollable
{
public:
    TimeProtocolConnection(
        StreamConnectionHolder<TimeProtocolConnection>* socketServer,
        std::unique_ptr<AbstractStreamSocket> _socket);
    ~TimeProtocolConnection();

    void startReadingConnection();

protected:
    virtual void stopWhileInAioThread() override;

private:
    StreamConnectionHolder<TimeProtocolConnection>* m_socketServer;
    std::unique_ptr<AbstractStreamSocket> m_socket;
    nx::Buffer m_outputBuffer;

    void onDataSent(
        SystemError::ErrorCode errorCode,
        size_t bytesSent);
};

} // namespace time_server
} // namespace nx
