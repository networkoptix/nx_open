// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    const auto utcTime = nx::utils::millisSinceEpoch();

    NX_VERBOSE(this, "Sending %1 UTC time to %2",
        utcTime, m_socket->getForeignAddress());

    // To preserve compatibility with rfc868 first sending seconds, then - milliseconds.
    std::uint32_t utcTimeSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(utcTime).count();
    utcTimeSeconds += network::kSecondsFrom1900_01_01To1970_01_01;
    utcTimeSeconds = htonl(utcTimeSeconds);

    std::uint32_t utcTimeMillis = htonl(utcTime.count() % 1000);

    m_outputBuffer.reserve(sizeof(utcTimeSeconds) + sizeof(utcTimeMillis));
    m_outputBuffer.append((const char*) &utcTimeSeconds, sizeof(utcTimeSeconds));
    m_outputBuffer.append((const char*) &utcTimeMillis, sizeof(utcTimeMillis));

    if (!m_socket->setNonBlockingMode(true))
        return triggerConnectionClosedEvent(SystemError::getLastOSErrorCode());

    connectionStatistics.messageReceived();

    m_socket->sendAsync(
        &m_outputBuffer,
        std::bind(&TimeProtocolConnection::onDataSent, this, _1, _2));
}

void TimeProtocolConnection::registerCloseHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, bool)> handler)
{
    m_connectionClosedHandlers.push_back(std::move(handler));
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
        NX_VERBOSE(this, nx::format("Failed to send to %1. %2")
            .arg(m_socket->getForeignAddress())
            .arg(SystemError::toString(errorCode)));
    }

    triggerConnectionClosedEvent(errorCode);
}

void TimeProtocolConnection::triggerConnectionClosedEvent(SystemError::ErrorCode reason)
{
    auto handlers = std::exchange(m_connectionClosedHandlers, {});
    nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
    for (auto& handler: handlers)
        handler(reason, watcher.interrupted());
}

} // namespace network
} // namespace nx
