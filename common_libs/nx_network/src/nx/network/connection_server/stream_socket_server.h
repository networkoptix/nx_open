#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <set>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/async_stoppable.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "detail/server_statistics_calculator.h"
#include "server_statistics.h"

namespace nx {
namespace network {
namespace server {

template<class _ConnectionType>
class StreamConnectionHolder
{
public:
    using ConnectionType = _ConnectionType;

    virtual ~StreamConnectionHolder() = default;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection) = 0;
};

template<class ConnectionType>
class StreamServerConnectionHolder:
    public StreamConnectionHolder<ConnectionType>
{
public:
    StreamServerConnectionHolder():
        m_connectionsBeingClosedCount(0)
    {
    }

    virtual ~StreamServerConnectionHolder()
    {
        // We MUST be sure to remove all connections.
        std::map<ConnectionType*, std::shared_ptr<ConnectionType>> connections;
        {
            QnMutexLocker lk(&m_mutex);
            connections.swap(m_connections);
        }
        for (auto& connection: connections)
            connection.first->pleaseStopSync();
        connections.clear();

        // Waiting connections being cancelled through closeConnection call to finish...
        QnMutexLocker lk(&m_mutex);
        while (m_connectionsBeingClosedCount > 0)
            m_cond.wait(lk.mutex());
    }

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        ConnectionType* connection) override
    {
        QnMutexLocker lk(&m_mutex);
        auto connectionIter = m_connections.find(connection);
        if (connectionIter == m_connections.end())
            return;
        auto connectionCtx = std::move(*connectionIter);
        m_connections.erase(connectionIter);

        ++m_connectionsBeingClosedCount;
        lk.unlock();

        //we are in connection's aio thread, so we can just free connection
        connectionCtx.second.reset();

        lk.relock();
        --m_connectionsBeingClosedCount;
        m_cond.wakeAll();
    }

    std::size_t connectionCount() const
    {
        QnMutexLocker lk(&m_mutex);
        return m_connections.size();
    }

    void saveConnection(std::shared_ptr<ConnectionType> connection)
    {
        QnMutexLocker lk(&m_mutex);
        m_connections.emplace(connection.get(), std::move(connection));
    }

    template<typename Func>
    void forEachConnection(Func func)
    {
        QnMutexLocker lk(&m_mutex);
        for (const auto& connection: m_connections)
            func(connection.first);
    }

private:
    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
    int m_connectionsBeingClosedCount;
    //TODO #ak this map types seems strange. Replace with std::set?
    std::map<ConnectionType*, std::shared_ptr<ConnectionType>> m_connections;
};

template<typename Connection>
struct MessageSender
{
    MessageSender(typename Connection::MessageType message):
        m_message(std::move(message))
    {
    }

    void operator()(Connection* connection)
    {
        connection->post(
            [message = m_message, connection]() mutable
            {
                connection->sendMessage(
                    std::move(message),
                    [](SystemError::ErrorCode) {});
            });
    }

private:
    typename Connection::MessageType m_message;
};

// TODO: #ak It seems to make sense to decouple
//   StreamSocketServer & StreamServerConnectionHolder responsibility.

/**
 * Listens local tcp address, accepts incoming connections
 *   and forwards them to the specified handler.
 */
template<class CustomServerType, class ConnectionType>
class StreamSocketServer:
    public StreamServerConnectionHolder<ConnectionType>,
    public aio::BasicPollable
{
    using base_type = StreamServerConnectionHolder<ConnectionType>;
    using self_type = StreamSocketServer<CustomServerType, ConnectionType>;

public:
    StreamSocketServer(std::unique_ptr<AbstractStreamServerSocket> serverSocket):
        m_socket(std::move(serverSocket))
    {
        bindToAioThread(getAioThread());
    }

    StreamSocketServer(
        bool sslRequired,
        nx::network::NatTraversalSupport natTraversalSupport)
        :
        StreamSocketServer(SocketFactory::createStreamServerSocket(
            sslRequired,
            natTraversalSupport))
    {
    }

    virtual ~StreamSocketServer()
    {
        pleaseStopSync(false);
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        aio::BasicPollable::bindToAioThread(aioThread);
        m_socket->bindToAioThread(aioThread);
    }

    bool bind(const SocketAddress& socketAddress)
    {
        return
            m_socket->setRecvTimeout(0) &&
            m_socket->setReuseAddrFlag(true) &&
            m_socket->bind(socketAddress);
    }

    bool listen(int backlogSize = AbstractStreamServerSocket::kDefaultBacklogSize)
    {
        using namespace std::placeholders;

        if (!m_socket->setNonBlockingMode(true) ||
            !m_socket->listen(backlogSize))
        {
            return false;
        }
        m_socket->acceptAsync(
            std::bind(&StreamSocketServer::newConnectionAccepted, this, _1, _2));
        return true;
    }

    SocketAddress address() const
    {
        return m_socket->getLocalAddress();
    }

    void setConnectionInactivityTimeout(boost::optional<std::chrono::milliseconds> value)
    {
        m_connectionInactivityTimeout = value;
    }

    void setConnectionKeepAliveOptions(boost::optional<KeepAliveOptions> options)
    {
        m_keepAliveOptions = std::move(options);
    }

    Statistics statistics() const
    {
        return m_statisticsCalculator.statistics(
            static_cast<int>(connectionCount()));
    }

protected:
    virtual std::shared_ptr<ConnectionType> createConnection(
        std::unique_ptr<AbstractStreamSocket> streamSocket) = 0;

    virtual void stopWhileInAioThread() override
    {
        m_socket.reset();
    }

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection) override
    {
        m_statisticsCalculator.saveConnectionStatistics(
            connection->lifeDuration(),
            connection->messagesReceivedCount());

        base_type::closeConnection(closeReason, connection);
    }

private:
    std::unique_ptr<AbstractStreamServerSocket> m_socket;
    boost::optional<std::chrono::milliseconds> m_connectionInactivityTimeout;
    boost::optional<KeepAliveOptions> m_keepAliveOptions;
    detail::StatisticsCalculator m_statisticsCalculator;

    StreamSocketServer(StreamSocketServer&);
    StreamSocketServer& operator=(const StreamSocketServer&);

    void newConnectionAccepted(
        SystemError::ErrorCode code,
        std::unique_ptr<AbstractStreamSocket> socket)
    {
        // TODO: #ak handle errorCode: try to call acceptAsync after some delay?
        m_socket->acceptAsync(
            [this](
                SystemError::ErrorCode code,
                std::unique_ptr<AbstractStreamSocket> socket)
            {
                newConnectionAccepted(code, std::move(socket));
            });

        if (code != SystemError::noError)
        {
            NX_LOGX(lm("Accept has failed: %1")
                .arg(SystemError::toString(code)), cl_logWARNING);
            return;
        }

        m_statisticsCalculator.connectionAccepted();

        if (m_keepAliveOptions)
        {
            const auto isKeepAliveSet = socket->setKeepAlive(m_keepAliveOptions);
            NX_ASSERT(isKeepAliveSet, SystemError::getLastOSErrorText());
        }

        auto connection = createConnection(std::move(socket));
        connection->startReadingConnection(m_connectionInactivityTimeout);
        this->saveConnection(std::move(connection));
    }
};

} // namespace server
} // namespace network
} // namespace nx
