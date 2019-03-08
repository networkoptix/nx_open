#pragma once

#include <chrono>

#include <nx/network/aio/basic_pollable.h>

namespace nx {
namespace network {

class NX_NETWORK_API TimeProtocolConnection:
    public network::aio::BasicPollable
{
public:
    TimeProtocolConnection(std::unique_ptr<AbstractStreamSocket> _socket);

    void startReadingConnection(std::optional<std::chrono::milliseconds> inactivityTimeout);

    void registerCloseHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    std::chrono::milliseconds lifeDuration() const;
    int messagesReceivedCount() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    nx::Buffer m_outputBuffer;
    std::chrono::steady_clock::time_point m_creationTimestamp;
    std::vector<nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>> m_connectionClosedHandlers;

    void onDataSent(
        SystemError::ErrorCode errorCode,
        size_t bytesSent);

    void triggerConnectionClosedEvent(SystemError::ErrorCode reason);
};

} // namespace network
} // namespace nx
