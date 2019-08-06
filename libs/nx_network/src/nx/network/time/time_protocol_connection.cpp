#include "time_protocol_connection.h"

#include <functional>

#include <nx/network/time/time_protocol_client.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

namespace nx {
namespace network {

TimeProtocolConnection::TimeProtocolConnection(
    std::unique_ptr<AbstractStreamSocket> _socket)
    :
    m_socket(std::move(_socket)),
    m_creationTimestamp(std::chrono::steady_clock::now())
{
    bindToAioThread(m_socket->getAioThread());
}

void TimeProtocolConnection::startReadingConnection(
    std::optional<std::chrono::milliseconds> inactivityTimeout)
{
    NX_ASSERT(!inactivityTimeout);
    using namespace std::placeholders;

    std::uint32_t utcTimeSeconds = nx::utils::timeSinceEpoch().count();

    NX_VERBOSE(this, lm("Sending %1 UTC time to %2")
        .arg(utcTimeSeconds).arg(m_socket->getForeignAddress()));

    utcTimeSeconds += network::kSecondsFrom1900_01_01To1970_01_01;
    utcTimeSeconds = htonl(utcTimeSeconds);
    m_outputBuffer.resize(sizeof(utcTimeSeconds));
    memcpy(m_outputBuffer.data(), &utcTimeSeconds, sizeof(utcTimeSeconds));

    if (!m_socket->setNonBlockingMode(true))
        return triggerConnectionClosedEvent(SystemError::getLastOSErrorCode());

    m_socket->sendAsync(
        m_outputBuffer,
        std::bind(&TimeProtocolConnection::onDataSent, this, _1, _2));
}

void TimeProtocolConnection::registerCloseHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_connectionClosedHandlers.push_back(std::move(handler));
}

std::chrono::milliseconds TimeProtocolConnection::lifeDuration() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now() - m_creationTimestamp);
}

int TimeProtocolConnection::messagesReceivedCount() const
{
    return 1;
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
        NX_VERBOSE(this, lm("Failed to send to %1. %2")
            .arg(m_socket->getForeignAddress())
            .arg(SystemError::toString(errorCode)));
    }

    triggerConnectionClosedEvent(errorCode);
}

void TimeProtocolConnection::triggerConnectionClosedEvent(SystemError::ErrorCode reason)
{
    auto handlers = std::exchange(m_connectionClosedHandlers, {});
    for (auto& handler: handlers)
        handler(reason);
}

} // namespace network
} // namespace nx
