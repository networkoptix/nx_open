#pragma once

#include <memory>

#include <QtCore/QByteArray>

#include <nx/network/abstract_socket.h>

#include "abstract_accurate_time_fetcher.h"

namespace nx {
namespace network {

constexpr const unsigned int kSecondsFrom19000101To19700101 = 2208988800UL;
constexpr const unsigned short kTimeProtocolDefaultPort = 37;     //time protocol

/**
 * Converts time from Time protocol format (rfc868) to millis from epoch (1970-01-01) UTC.
 */
NX_NETWORK_API qint64 rfc868TimestampToTimeToUtcMillis(const QByteArray& timeStr);

/**
 * Fetches time using Time (rfc868) protocol.
 * Result time is accurate to second boundary only.
 */
class NX_NETWORK_API TimeProtocolClient:
    public AbstractAccurateTimeFetcher
{
public:
    /** Uses timeServerHost:{standard_time_protocol_port} */
    TimeProtocolClient(const QString& timeServerHost);
    TimeProtocolClient(const SocketAddress& timeServerEndpoint);

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread);

    virtual void getTimeAsync(CompletionHandler completionHandler) override;

private:
    const SocketAddress m_timeServerEndpoint;
    std::unique_ptr<AbstractStreamSocket> m_tcpSock;
    QByteArray m_timeStr;
    CompletionHandler m_completionHandler;

    void getTimeAsyncInAioThread(CompletionHandler completionHandler);
    void onConnectionEstablished(SystemError::ErrorCode errorCode);
    void onSomeBytesRead(SystemError::ErrorCode errorCode, size_t bytesRead);
    void reportResult(qint64 timeMillis, SystemError::ErrorCode sysErrorCode);
};

} // namespace network
} // namespace nx
