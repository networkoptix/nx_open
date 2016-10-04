#include "time_protocol_connection.h"

#include <functional>

#include <nx/network/time/time_protocol_client.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace time_server {

TimeProtocolConnection::TimeProtocolConnection(
    StreamConnectionHolder<TimeProtocolConnection>* socketServer,
    std::unique_ptr<AbstractStreamSocket> _socket)
    :
    m_socketServer(socketServer),
    m_socket(std::move(_socket))
{
    bindToAioThread(m_socket->getAioThread());
}

TimeProtocolConnection::~TimeProtocolConnection()
{
    stopWhileInAioThread();
}

void TimeProtocolConnection::startReadingConnection()
{
    using namespace std::placeholders;

    std::uint32_t utcTimeSeconds = ::time(NULL);
    //std::uint32_t utcTimeSeconds = 
    //    std::chrono::duration_cast<std::chrono::seconds>(
    //        std::chrono::system_clock::now().time_since_epoch()).count();

    NX_LOGX(lm("Sending %1 UTC time to %2")
        .arg(utcTimeSeconds).str(m_socket->getForeignAddress()),
        cl_logDEBUG2);

    utcTimeSeconds += kSecondsFrom19000101To19700101;
    utcTimeSeconds = htonl(utcTimeSeconds);
    m_outputBuffer.resize(sizeof(utcTimeSeconds));
    memcpy(m_outputBuffer.data(), &utcTimeSeconds, sizeof(utcTimeSeconds));

    m_socket->sendAsync(
        m_outputBuffer,
        std::bind(&TimeProtocolConnection::onDataSent, this, _1, _2));
}

void TimeProtocolConnection::stopWhileInAioThread()
{
    m_socket.reset();
}

void TimeProtocolConnection::onDataSent(
    SystemError::ErrorCode errorCode,
    size_t /*bytesSent*/)
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("Failed to send to %1. %2")
            .str(m_socket->getForeignAddress())
            .arg(SystemError::toString(errorCode)),
            cl_logDEBUG2);
    }

    m_socketServer->closeConnection(errorCode, this);
}

} // namespace time_server
} // namespace nx
