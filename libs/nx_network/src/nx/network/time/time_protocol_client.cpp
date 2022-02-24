// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_protocol_client.h"

#include <sys/types.h>

#include <chrono>

#include <nx/network/socket_factory.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/time.h>

constexpr size_t kMaxTimeStrLength = sizeof(quint32) * 2;
constexpr std::chrono::seconds kSocketRecvTimeout = std::chrono::seconds(7);
constexpr int kMillisPerSec = 1000;

namespace nx::network {

std::optional<qint64> rfc868TimestampToTimeToUtcMillis(const std::string_view& timeStr)
{
    quint32 utcTimeSeconds = 0;
    if ((size_t)timeStr.size() < sizeof(utcTimeSeconds))
        return std::nullopt;

    memcpy(&utcTimeSeconds, timeStr.data(), sizeof(utcTimeSeconds));
    utcTimeSeconds = ntohl(utcTimeSeconds);
    utcTimeSeconds -= kSecondsFrom1900_01_01To1970_01_01;

    if ((size_t)timeStr.size() < kMaxTimeStrLength) //< Backward compatibility with rfc868.
        return ((qint64) utcTimeSeconds) * kMillisPerSec;

    quint32 utcTimeMillis = 0;
    memcpy(&utcTimeMillis, timeStr.data() + sizeof(utcTimeSeconds), sizeof(utcTimeMillis));
    utcTimeMillis = ntohl(utcTimeMillis);

    return ((qint64) utcTimeSeconds) * kMillisPerSec + utcTimeMillis;
}

//-------------------------------------------------------------------------------------------------

TimeProtocolClient::TimeProtocolClient(const HostAddress& timeServerHost):
    TimeProtocolClient(SocketAddress(timeServerHost, kTimeProtocolDefaultPort))
{
}

TimeProtocolClient::TimeProtocolClient(const SocketAddress& timeServerEndpoint):
    m_timeServerEndpoint(timeServerEndpoint)
{
    m_timeStr.reserve(kMaxTimeStrLength);
}

void TimeProtocolClient::stopWhileInAioThread()
{
    m_tcpSock.reset();
}

void TimeProtocolClient::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractAccurateTimeFetcher::bindToAioThread(aioThread);
    if (m_tcpSock)
        m_tcpSock->bindToAioThread(aioThread);
}

void TimeProtocolClient::getTimeAsync(
    CompletionHandler completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            getTimeAsyncInAioThread(std::move(completionHandler));
        });
}

void TimeProtocolClient::getTimeAsyncInAioThread(
    CompletionHandler completionHandler)
{
    NX_VERBOSE(this, nx::format("rfc868 time_sync. Starting time synchronization with %1:%2")
        .arg(m_timeServerEndpoint).arg(kTimeProtocolDefaultPort));

    m_completionHandler = std::move(completionHandler);

    m_tcpSock = SocketFactory::createStreamSocket(nx::network::ssl::kAcceptAnyCertificate);
    m_tcpSock->bindToAioThread(getAioThread());
    if (!m_tcpSock->setNonBlockingMode(true) ||
        !m_tcpSock->setRecvTimeout(kSocketRecvTimeout) ||
        !m_tcpSock->setSendTimeout(kSocketRecvTimeout))
    {
        post(std::bind(&TimeProtocolClient::reportResult, this,
            -1, SystemError::getLastOSErrorCode(), std::chrono::milliseconds::zero()));
        return;
    }

    using namespace std::placeholders;
    m_tcpSock->connectAsync(
        m_timeServerEndpoint,
        std::bind(&TimeProtocolClient::onConnectionEstablished, this, _1));
}

void TimeProtocolClient::onConnectionEstablished(
    SystemError::ErrorCode errorCode)
{
    NX_VERBOSE(this, nx::format("rfc868 time_sync. Connection to time server %1 "
        "completed with following result: %2")
        .arg(m_timeServerEndpoint).arg(SystemError::toString(errorCode)));

    if (errorCode)
    {
        reportResult(-1, errorCode, std::chrono::milliseconds::zero());
        return;
    }

    m_timeStr.reserve(kMaxTimeStrLength);
    m_timeStr.resize(0);
    m_elapsedTimer.restart();

    using namespace std::placeholders;
    m_tcpSock->readSomeAsync(
        &m_timeStr,
        std::bind(&TimeProtocolClient::onSomeBytesRead, this, _1, _2));
}

void TimeProtocolClient::onSomeBytesRead(
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    using namespace std::placeholders;

    // TODO: #akolesnikov Take into account rtt/2.

    if (errorCode)
    {
        NX_DEBUG(this, nx::format("rfc868 time_sync. Failed to recv from %1. %2")
            .arg(m_timeServerEndpoint).arg(SystemError::toString(errorCode)));

        reportResult(-1, errorCode, std::chrono::milliseconds::zero());
        return;
    }

    if (bytesRead == 0)
    {
        NX_VERBOSE(this, nx::format("rfc868 time_sync. Connection to %1 has been closed. Received just %2 bytes")
            .arg(m_timeServerEndpoint).arg(m_timeStr.size()));

        // Trying to return hat we have.
        if (auto time = rfc868TimestampToTimeToUtcMillis(m_timeStr); time != std::nullopt)
            reportResult(*time, SystemError::noError, m_elapsedTimer.elapsed());
        else
            reportResult(-1, SystemError::notConnected, std::chrono::milliseconds::zero());
        return;
    }

    NX_VERBOSE(this, nx::format("rfc868 time_sync. Read %1 bytes from time server %2").
        arg(bytesRead).arg(m_timeServerEndpoint));

    if (m_timeStr.size() >= kMaxTimeStrLength)
    {
        NX_DEBUG(this, nx::format("rfc868 time_sync. Read %1 from time server %2").
            arg(nx::utils::toHex(m_timeStr)).arg(m_timeServerEndpoint));

        // Max data size has been read, ignoring futher data.
        reportResult(
            *rfc868TimestampToTimeToUtcMillis((std::string_view) m_timeStr),
            SystemError::noError,
            m_elapsedTimer.elapsed());
        return;
    }

    // Reading futher data.
    m_tcpSock->readSomeAsync(
        &m_timeStr,
        std::bind(&TimeProtocolClient::onSomeBytesRead, this, _1, _2));
}

void TimeProtocolClient::reportResult(
    qint64 timeMs,
    SystemError::ErrorCode sysErrorCode,
    std::chrono::milliseconds rtt)
{
    m_tcpSock.reset();
    m_completionHandler(timeMs, sysErrorCode, rtt);
}

} // namespace nx::network
