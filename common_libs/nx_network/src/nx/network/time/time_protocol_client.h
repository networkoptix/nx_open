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
 * Fetches time using Time (rfc868) protocol.
 * Result time is accurate to second boundary only.
 */
class NX_NETWORK_API TimeProtocolClient:
    public AbstractAccurateTimeFetcher
{
public:
    TimeProtocolClient(const QString& timeServerHost);

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
};

} // namespace network
} // namespace nx
