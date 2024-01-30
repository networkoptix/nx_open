// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <type_traits>

#include <nx/network/abstract_socket.h>
#include <nx/network/abstract_stream_socket_acceptor.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/async_stoppable.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_factory.h>
#include <nx/network/stream_server_socket_to_acceptor_wrapper.h>
#include <nx/reflect/type_utils.h>
#include <nx/utils/log/log.h>
#include <nx/utils/member_detector.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "detail/connection_statistics.h"
#include "detail/server_statistics_calculator.h"

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

//-------------------------------------------------------------------------------------------------

template<typename ConnectionType>
class StreamServerConnectionHolder:
    public StreamConnectionHolder<ConnectionType>,
    public AbstractStatisticsProvider
{
private:
    template <typename T, typename = void>
    struct HasConnectionStatistics: std::false_type {};

    template<typename T>
    struct HasConnectionStatistics<
        T,
        std::enable_if_t<
            std::is_same_v<
                detail::ConnectionStatistics,
                decltype(std::declval<T>().connectionStatistics)
            >
        >
    >: std::true_type {};

public:
    virtual ~StreamServerConnectionHolder()
    {
        closeAllConnections();
    }

    virtual Statistics statistics() const override
    {
        return m_statisticsCalculator.statistics(
            static_cast<int>(this->connectionCount()));
    }

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        ConnectionType* connection) override
    {
        NX_MUTEX_LOCKER lk(&m_mutex);

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
        NX_MUTEX_LOCKER lk(&m_mutex);
        return m_connections.size();
    }

    void saveConnection(std::shared_ptr<ConnectionType> connection)
    {
        m_statisticsCalculator.connectionAccepted();

        if constexpr (HasConnectionStatistics<ConnectionType>::value)
        {
            connection->connectionStatistics.setMessageReceivedHandler(
                [this](){ m_statisticsCalculator.messageReceived(); });
        }

        auto connectionPtr = connection.get();
        if constexpr (registerCloseHandlerExists<ConnectionType>::value)
        {
            connection->registerCloseHandler(
                [this, connectionPtr](auto closeReason, auto /*connectionDestroyed*/)
                {
                    closeConnection(closeReason, connectionPtr);
                });
        }

        NX_MUTEX_LOCKER lk(&m_mutex);

        m_connections.emplace(connectionPtr, std::move(connection));
    }

    template<typename Func>
    void forEachConnection(Func func)
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        for (const auto& connection: m_connections)
            func(connection.first);
    }

    void closeAllConnections()
    {
        // We MUST be sure to remove all connections.
        std::map<ConnectionType*, std::shared_ptr<ConnectionType>> connections;
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            connections.swap(m_connections);
        }
        for (auto& connection: connections)
        {
            connection.first->executeInAioThreadSync(
                [conn = std::exchange(connection.second, nullptr)]() mutable
                {
                    // Someone else may have strong refs to the connection, so invoking
                    // pleaseStopSync() explicitly since reset() may not destroy the connection.
                    conn->pleaseStopSync();
                    conn.reset();
                });
        }
        connections.clear();

        // Waiting connections being cancelled through closeConnection call to finish...
        NX_MUTEX_LOCKER lk(&m_mutex);
        while (m_connectionsBeingClosedCount > 0)
            m_cond.wait(lk.mutex());
    }

private:
    NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(registerCloseHandlerExists, registerCloseHandler);

    mutable nx::Mutex m_mutex;
    nx::WaitCondition m_cond;
    int m_connectionsBeingClosedCount = 0;
    //TODO #akolesnikov this map types seems strange. Replace with std::set?
    std::map<ConnectionType*, std::shared_ptr<ConnectionType>> m_connections;
    detail::StatisticsCalculator m_statisticsCalculator;
};

//-------------------------------------------------------------------------------------------------

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

//-------------------------------------------------------------------------------------------------

// TODO: #akolesnikov It seems to make sense to decouple
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
        m_serverSocket(serverSocket.get()),
        m_acceptor(
            std::make_unique<StreamServerSocketToAcceptorWrapper>(
                std::move(serverSocket)))
    {
        bindToAioThread(m_acceptor->getAioThread());
    }

    StreamSocketServer(std::unique_ptr<AbstractStreamSocketAcceptor> acceptor):
        m_acceptor(std::move(acceptor))
    {
        bindToAioThread(m_acceptor->getAioThread());
    }

    /**
     * Initializes regular (non-SSL) connection listener.
     */
    StreamSocketServer():
        StreamSocketServer(SocketFactory::createStreamServerSocket(/*sslRequired*/ false))
    {
    }

    /**
     * Initializes SSL connection listener.
     */
    StreamSocketServer(ssl::Context* sslContext):
        StreamSocketServer(
            SocketFactory::createSslAdapter(
                SocketFactory::createStreamServerSocket(/*sslRequired*/ false),
                sslContext,
                ssl::EncryptionUse::autoDetectByReceivedData))
    {
    }

    virtual ~StreamSocketServer()
    {
        pleaseStopSync();
        this->closeAllConnections();
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        aio::BasicPollable::bindToAioThread(aioThread);
        m_acceptor->bindToAioThread(aioThread);
    }

    /**
     * Set to true to allow binding multiple StreamSocketServer to the same address:port.
     */
    bool setReusePort(bool val)
    {
        return m_serverSocket->setReuseAddrFlag(val) && m_serverSocket->setReusePortFlag(val);
    }

    bool bind(const SocketAddress& socketAddress)
    {
        NX_CRITICAL(m_serverSocket);

        return
            m_serverSocket->setRecvTimeout(0) &&
            m_serverSocket->setReuseAddrFlag(true) &&
            m_serverSocket->bind(socketAddress);
    }

    bool listen(int backlogSize = AbstractStreamServerSocket::kDefaultBacklogSize)
    {
        NX_CRITICAL(m_serverSocket);

        if (!m_serverSocket->setNonBlockingMode(true) ||
            !m_serverSocket->listen(backlogSize))
        {
            return false;
        }

        start();
        return true;
    }

    SocketAddress address() const
    {
        NX_CRITICAL(m_serverSocket);
        return m_serverSocket->getLocalAddress();
    }

    void start()
    {
        using namespace std::placeholders;

        m_acceptor->acceptAsync(
            std::bind(&StreamSocketServer::newConnectionAccepted, this, _1, _2));
    }

    void setConnectionInactivityTimeout(std::optional<std::chrono::milliseconds> value)
    {
        m_connectionInactivityTimeout = value;
    }

    void setConnectionKeepAliveOptions(std::optional<KeepAliveOptions> options)
    {
        m_keepAliveOptions = std::move(options);
    }

protected:
    virtual std::shared_ptr<ConnectionType> createConnection(
        std::unique_ptr<AbstractStreamSocket> streamSocket) = 0;

    virtual void stopWhileInAioThread() override
    {
        m_acceptor->pleaseStopSync();
        m_serverSocket = nullptr;
    }

private:
    // TODO: #akolesnikov Deal with m_serverSocket & m_acceptor.
    // The idea is to use this class with AbstractStreamSocketAcceptor only.
    // That in turn will allow using HttpServer with any acceptor (e.g., relay/reverse acceptor).
    // CLOUD-1925.

    AbstractStreamServerSocket* m_serverSocket = nullptr;
    std::unique_ptr<AbstractStreamSocketAcceptor> m_acceptor;
    std::optional<std::chrono::milliseconds> m_connectionInactivityTimeout;
    std::optional<KeepAliveOptions> m_keepAliveOptions;

    StreamSocketServer(StreamSocketServer&);
    StreamSocketServer& operator=(const StreamSocketServer&);

    void newConnectionAccepted(
        SystemError::ErrorCode code,
        std::unique_ptr<AbstractStreamSocket> socket)
    {
        // TODO: #akolesnikov handle errorCode: try to call acceptAsync after some delay?
        m_acceptor->acceptAsync(
            [this](
                SystemError::ErrorCode code,
                std::unique_ptr<AbstractStreamSocket> socket)
            {
                newConnectionAccepted(code, std::move(socket));
            });

        if (code != SystemError::noError)
        {
            NX_WARNING(this, nx::format("Accept has failed: %1")
                .arg(SystemError::toString(code)));
            return;
        }

        if (!socket->setNonBlockingMode(true))
        {
            const auto errorCode = SystemError::getLastOSErrorCode();
            NX_WARNING(this, nx::format("Failed to switch socket to a non blocking mode: %1")
                .arg(SystemError::toString(errorCode)));
            return;
        }

        if (m_keepAliveOptions)
        {
            const auto isKeepAliveSet = socket->setKeepAlive(m_keepAliveOptions);
            NX_ASSERT(isKeepAliveSet, SystemError::getLastOSErrorText());
        }

        auto connection = createConnection(std::move(socket));
        auto connectionPtr = connection.get();
        this->saveConnection(std::move(connection));
        // It is guaranteed that connection is alive here because
        // connection can be deleted by an event from itself only.
        // And event can arrive not sooner than connection is started.
        connectionPtr->startReadingConnection(m_connectionInactivityTimeout);
    }
};

} // namespace server
} // namespace network
} // namespace nx
