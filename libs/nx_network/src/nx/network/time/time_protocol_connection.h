// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/interruption_flag.h>

namespace nx {
namespace network {

class NX_NETWORK_API TimeProtocolConnection:
    public network::aio::BasicPollable
{
public:
    TimeProtocolConnection(std::unique_ptr<AbstractStreamSocket> _socket);

    void startReadingConnection(std::optional<std::chrono::milliseconds> inactivityTimeout);

    void registerCloseHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, bool /*connectionDestroyed*/)> handler);

    std::chrono::milliseconds lifeDuration() const;
    int messagesReceivedCount() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    nx::Buffer m_outputBuffer;
    std::chrono::steady_clock::time_point m_creationTimestamp;
    std::vector<nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, bool)>> m_connectionClosedHandlers;
    nx::utils::InterruptionFlag m_connectionFreedFlag;

    void onDataSent(
        SystemError::ErrorCode errorCode,
        size_t bytesSent);

    void triggerConnectionClosedEvent(SystemError::ErrorCode reason);
};

} // namespace network
} // namespace nx
