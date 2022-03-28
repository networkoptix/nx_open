// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <queue>

#include "stream_socket_server.h"
#include "detail/connection_statistics.h"

#include <nx/utils/interruption_flag.h>

namespace nx::network::server {

class NX_NETWORK_API SimpleMessageServerConnection:
    public network::aio::BasicPollable
{
public:
    detail::ConnectionStatistics connectionStatistics;

    SimpleMessageServerConnection(
        std::unique_ptr<AbstractStreamSocket> _socket,
        nx::Buffer request,
        nx::Buffer response);

    void startReadingConnection(
        std::optional<std::chrono::milliseconds> /*inactivityTimeout*/);

    void registerCloseHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, bool /*connectionDestroyed*/)> handler);

    void setKeepConnection(bool val);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    nx::Buffer m_request;
    nx::Buffer m_response;
    nx::Buffer m_readBuffer;
    const std::chrono::steady_clock::time_point m_creationTimestamp;
    bool m_keepConnection = false;
    std::queue<nx::Buffer> m_sendQueue;
    std::vector<nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, bool)>> m_connectionClosedHandlers;
    nx::utils::InterruptionFlag m_connectionFreedFlag;

    void onDataRead(
        SystemError::ErrorCode errorCode,
        size_t bytesRead);

    void scheduleMessageSend();

    void sendNextMessage();

    void onDataSent(
        SystemError::ErrorCode errorCode,
        size_t bytesSent);

    void triggerConnectionClosedEvent(SystemError::ErrorCode reason);
};

//-------------------------------------------------------------------------------------------------

/**
 * Sends predefined response after receiving expected request via every connection accepted.
 * If no request is defined, than response is sent immediately after accepting connection.
 */
class SimpleMessageServer:
    public server::StreamSocketServer<SimpleMessageServer, SimpleMessageServerConnection>
{
    using base_type =
        server::StreamSocketServer<SimpleMessageServer, SimpleMessageServerConnection>;

public:
    template<typename... Args>
    SimpleMessageServer(Args&&... args):
        base_type(std::forward<Args>(args)...)
    {
    }

    void setRequest(nx::Buffer message)
    {
        m_request.swap(message);
    }

    void setResponse(nx::Buffer message)
    {
        m_response.swap(message);
    }

    void setKeepConnection(bool val)
    {
        m_keepConnection = val;
    }

protected:
    virtual std::shared_ptr<SimpleMessageServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        auto connection = std::make_shared<SimpleMessageServerConnection>(
            std::move(_socket),
            m_request,
            m_response);
        connection->setKeepConnection(m_keepConnection);
        return connection;
    }

private:
    nx::Buffer m_request;
    nx::Buffer m_response;
    bool m_keepConnection = false;
};

} // namespace nx::network::server
