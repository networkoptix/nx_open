#include "time_protocol_client.h"

#include <sys/timeb.h>
#include <sys/types.h>

#include <chrono>

#include <nx/network/socket_factory.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/time.h>

constexpr size_t kMaxTimeStrLength = sizeof(quint32);
constexpr std::chrono::seconds kSocketRecvTimeout = std::chrono::seconds(7);
constexpr int kMillisPerSec = 1000;

namespace nx {
namespace network {

qint64 rfc868TimestampToTimeToUtcMillis(const QByteArray& timeStr)
{
    quint32 utcTimeSeconds = 0;
    if ((size_t)timeStr.size() < sizeof(utcTimeSeconds))
        return -1;
    memcpy(&utcTimeSeconds, timeStr.constData(), sizeof(utcTimeSeconds));
    utcTimeSeconds = ntohl(utcTimeSeconds);
    utcTimeSeconds -= kSecondsFrom1900_01_01To1970_01_01;
    return ((qint64) utcTimeSeconds) * kMillisPerSec;
}

//-------------------------------------------------------------------------------------------------

TimeProtocolClient::TimeProtocolClient(const QString& timeServerHost):
    m_timeServerEndpoint(timeServerHost, kTimeProtocolDefaultPort)
{
    m_timeStr.reserve(kMaxTimeStrLength);
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
    NX_LOGX(lm("rfc868 time_sync. Starting time synchronization with %1:%2")
        .arg(m_timeServerEndpoint).arg(kTimeProtocolDefaultPort),
        cl_logDEBUG2);

    m_completionHandler = std::move(completionHandler);

    m_tcpSock = SocketFactory::createStreamSocket(false);
    m_tcpSock->bindToAioThread(getAioThread());
    if (!m_tcpSock->setNonBlockingMode(true) ||
        !m_tcpSock->setRecvTimeout(kSocketRecvTimeout) ||
        !m_tcpSock->setSendTimeout(kSocketRecvTimeout))
    {
        post(std::bind(&TimeProtocolClient::reportResult, this,
            -1, SystemError::getLastOSErrorCode()));
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
    NX_LOGX(lm("rfc868 time_sync. Connection to time server %1 "
            "completed with following result: %2")
            .arg(m_timeServerEndpoint).arg(SystemError::toString(errorCode)),
        cl_logDEBUG2);

    if (errorCode)
    {
        reportResult(-1, errorCode);
        return;
    }

    m_timeStr.reserve(kMaxTimeStrLength);
    m_timeStr.resize(0);

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

    // TODO: #ak Take into account rtt/2.

    if (errorCode)
    {
        NX_LOGX(lm("rfc868 time_sync. Failed to recv from %1. %2")
            .arg(m_timeServerEndpoint).arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);

        reportResult(-1, errorCode);
        return;
    }

    if (bytesRead == 0)
    {
        NX_LOGX(lm("rfc868 time_sync. Connection to %1 has been closed. Received just %2 bytes")
            .arg(m_timeServerEndpoint).arg(m_timeStr.size()),
            cl_logDEBUG2);

        reportResult(-1, SystemError::notConnected);
        return;
    }

    NX_LOGX(lm("rfc868 time_sync. Read %1 bytes from time server %2").
        arg(bytesRead).arg(m_timeServerEndpoint), cl_logDEBUG2);

    if (m_timeStr.size() >= m_timeStr.capacity())
    {
        NX_LOGX(lm("rfc868 time_sync. Read %1 from time server %2").
            arg(m_timeStr.toHex()).arg(m_timeServerEndpoint),
            cl_logDEBUG1);

        // Max data size has been read, ignoring futher data.
        reportResult(
            rfc868TimestampToTimeToUtcMillis(m_timeStr),
            SystemError::noError);
        return;
    }

    // Reading futher data.
    m_tcpSock->readSomeAsync(
        &m_timeStr,
        std::bind(&TimeProtocolClient::onSomeBytesRead, this, _1, _2));
}

void TimeProtocolClient::reportResult(
    qint64 timeMillis,
    SystemError::ErrorCode sysErrorCode)
{
    m_tcpSock.reset();
    m_completionHandler(timeMillis, sysErrorCode);
}

} // namespace network
} // namespace nx
