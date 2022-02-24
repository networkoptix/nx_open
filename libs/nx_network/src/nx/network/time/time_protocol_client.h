// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <nx/network/abstract_socket.h>
#include <nx/utils/elapsed_timer.h>

#include "abstract_accurate_time_fetcher.h"

namespace nx::network {

static constexpr unsigned int kSecondsFrom1900_01_01To1970_01_01 = 2208988800UL;
static constexpr unsigned short kTimeProtocolDefaultPort = 37;     //time protocol

/**
 * Converts time from Time protocol format (rfc868) to millis from epoch (1970-01-01) UTC.
 */
NX_NETWORK_API std::optional<qint64> rfc868TimestampToTimeToUtcMillis(const std::string_view& timeStr);

/**
 * Fetches time using Time (rfc868) protocol.
 * Result time is accurate to second boundary only.
 */
class NX_NETWORK_API TimeProtocolClient:
    public AbstractAccurateTimeFetcher
{
public:
    /** Uses timeServerHost:{standard_time_protocol_port} */
    explicit TimeProtocolClient(const HostAddress& timeServerHost);
    explicit TimeProtocolClient(const SocketAddress& timeServerEndpoint);

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void getTimeAsync(CompletionHandler completionHandler) override;

private:
    const SocketAddress m_timeServerEndpoint;
    std::unique_ptr<AbstractStreamSocket> m_tcpSock;
    nx::Buffer m_timeStr;
    CompletionHandler m_completionHandler;

    void getTimeAsyncInAioThread(CompletionHandler completionHandler);
    void onConnectionEstablished(SystemError::ErrorCode errorCode);
    void onSomeBytesRead(SystemError::ErrorCode errorCode, size_t bytesRead);

    void reportResult(
        qint64 timeMillis,
        SystemError::ErrorCode sysErrorCode,
        std::chrono::milliseconds rtt);

    nx::utils::ElapsedTimer m_elapsedTimer;
};

} // namespace nx::network
