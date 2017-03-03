/**********************************************************
* Feb 4, 2016
* akolesnikov
***********************************************************/

#include <atomic>

#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_connection_watcher.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/test_options.h>
#include <utils/common/guard.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class DummyConnection:
    public AbstractOutgoingTunnelConnection
{
public:
    static std::atomic<size_t> instanceCount;
    static std::set<DummyConnection*> instances;
    static QnMutex instanceSetMutex;

    DummyConnection(
        const String& /*remotePeerId*/,
        bool connectionShouldWorkFine,
        bool singleShot,
        nx::utils::promise<std::chrono::milliseconds>* tunnelConnectionInvokedPromise = nullptr)
    :
        m_connectionShouldWorkFine(connectionShouldWorkFine),
        m_singleShot(singleShot),
        m_tunnelConnectionInvokedPromise(tunnelConnectionInvokedPromise),
        m_ignoreConnectRequest(false)
    {
        ++instanceCount;
        bindToAioThread(SocketGlobals::aioService().getCurrentAioThread());

        QnMutexLocker lock(&instanceSetMutex);
        instances.insert(this);
    }

    ~DummyConnection()
    {
        stopWhileInAioThread();
        --instanceCount;

        QnMutexLocker lock(&instanceSetMutex);
        instances.erase(this);
    }

    virtual void stopWhileInAioThread() override
    {
    }

    virtual void start() override
    {
    }

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes /*socketAttributes*/,
        OnNewConnectionHandler handler) override
    {
        if (m_ignoreConnectRequest)
            return;

        if (m_tunnelConnectionInvokedPromise)
        {
            m_tunnelConnectionInvokedPromise->set_value(timeout);
            m_tunnelConnectionInvokedPromise = nullptr;
        }

        post(
            [this, handler = std::move(handler)]()
            {
                if (m_connectionShouldWorkFine)
                {
                    handler(
                        SystemError::noError,
                        std::make_unique<TCPSocket>(AF_INET),
                        !m_singleShot);

                    if (m_singleShot)
                        m_connectionShouldWorkFine = false;
                }
                else
                {
                    handler(SystemError::connectionReset, nullptr, false);
                }
            });
    }

    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        m_onClosedHandler = std::move(handler);
    }

    void ignoreConnectRequests()
    {
        m_ignoreConnectRequest = true;
    }

    void reportClosure(SystemError::ErrorCode sysErrorCode)
    {
        post([this, sysErrorCode](){ m_onClosedHandler(sysErrorCode); });
    }

private:
    bool m_connectionShouldWorkFine;
    const bool m_singleShot;
    nx::utils::promise<std::chrono::milliseconds>* m_tunnelConnectionInvokedPromise;
    bool m_ignoreConnectRequest;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onClosedHandler;
};

std::atomic<size_t> DummyConnection::instanceCount(0);
std::set<DummyConnection*> DummyConnection::instances;
QnMutex DummyConnection::instanceSetMutex;


class DummyConnector:
    public AbstractCrossNatConnector
{
public:
    static std::atomic<size_t> instanceCount;

    DummyConnector(
        AddressEntry targetPeerAddress,
        bool connectSuccessfully,
        bool connectionShouldWorkFine,
        bool singleShotConnection,
        nx::utils::promise<void>* const canSucceedEvent,
        boost::optional<std::chrono::milliseconds> connectTimeout)
    :
        m_targetPeerAddress(std::move(targetPeerAddress)),
        m_connectSuccessfully(connectSuccessfully),
        m_connectionShouldWorkFine(connectionShouldWorkFine),
        m_singleShotConnection(singleShotConnection),
        m_canSucceedEvent(canSucceedEvent),
        m_connectTimeout(std::move(connectTimeout)),
        m_tunnelConnectionInvokedPromise(nullptr),
        m_timer(std::make_unique<aio::Timer>())
    {
        m_timer->bindToAioThread(getAioThread());
        ++instanceCount;
    }

    DummyConnector(
        AddressEntry targetPeerAddress,
        bool connectSuccessfully,
        bool connectionShouldWorkFine,
        bool singleShotConnection)
    :
        DummyConnector(
            std::move(targetPeerAddress),
            connectSuccessfully,
            connectionShouldWorkFine,
            singleShotConnection,
            nullptr,
            boost::none)
    {
    }

    DummyConnector(
        AddressEntry targetPeerAddress,
        nx::utils::promise<void>* const canSucceedEvent)
    :
        DummyConnector(
            std::move(targetPeerAddress),
            true,
            true,
            false,
            canSucceedEvent,
            boost::none)
    {
    }

    DummyConnector(
        AddressEntry targetPeerAddress,
        std::chrono::milliseconds connectTimeout,
        nx::utils::promise<void>* const canSucceedEvent = nullptr)
    :
        DummyConnector(
            std::move(targetPeerAddress),
            true,
            true,
            false,
            canSucceedEvent,
            connectTimeout)
    {
    }

    ~DummyConnector()
    {
        stopWhileInAioThread();
        --instanceCount;
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        AbstractCrossNatConnector::bindToAioThread(aioThread);
        m_timer->bindToAioThread(getAioThread());
    }

    virtual void stopWhileInAioThread() override
    {
        m_timer.reset();
    }

    virtual void connect(
        std::chrono::milliseconds /*timeout*/,
        ConnectCompletionHandler handler) override
    {
        if (m_connectTimeout)
        {
            m_timer->start(
                *m_connectTimeout,
                [this, handler = std::move(handler)]() mutable
                {
                    connectInternal(std::move(handler));
                });
        }
        else
        {
            connectInternal(std::move(handler));
        }
    }

    void setConnectionInvokedPromise(
        nx::utils::promise<std::chrono::milliseconds>* const tunnelConnectionInvokedPromise)
    {
        m_tunnelConnectionInvokedPromise = tunnelConnectionInvokedPromise;
    }

private:
    AddressEntry m_targetPeerAddress;
    const bool m_connectSuccessfully;
    const bool m_connectionShouldWorkFine;
    const bool m_singleShotConnection;
    nx::utils::promise<void>* const m_canSucceedEvent;
    boost::optional<std::chrono::milliseconds> m_connectTimeout;
    nx::utils::promise<std::chrono::milliseconds>* m_tunnelConnectionInvokedPromise;
    std::unique_ptr<aio::Timer> m_timer;

    void connectInternal(
        ConnectCompletionHandler handler)
    {
        post(
            [this, handler = move(handler)]()
            {
                if (m_canSucceedEvent)
                    m_canSucceedEvent->get_future().wait();

                if (m_connectSuccessfully)
                {
                    auto dummyConnection = std::make_unique<DummyConnection>(
                        m_targetPeerAddress.host.toString().toLatin1(),
                        m_connectionShouldWorkFine,
                        m_singleShotConnection,
                        m_tunnelConnectionInvokedPromise);

                    auto tunnelWatcher = std::make_unique<OutgoingTunnelConnectionWatcher>(
                        std::move(nx::hpm::api::ConnectionParameters()),
                        std::move(dummyConnection));
                    tunnelWatcher->bindToAioThread(getAioThread());
                    tunnelWatcher->start();

                    handler(
                        SystemError::noError,
                        std::move(tunnelWatcher));
                }
                else
                {
                    handler(
                        SystemError::connectionRefused,
                        std::unique_ptr<DummyConnection>());
                }
            });
    }
};

std::atomic<size_t> DummyConnector::instanceCount(0);


class OutgoingTunnel:
    public ::testing::Test
{
public:
    typedef nx::utils::promise<std::pair<
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket >> > ConnectionCompletedPromise;

    struct ConnectionContext
    {
        ConnectionCompletedPromise completionPromise;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
    };

    OutgoingTunnel()
    {
        NX_CRITICAL(DummyConnector::instanceCount == 0);
        NX_CRITICAL(DummyConnection::instanceCount == 0);
    }

    ~OutgoingTunnel()
    {
        if (m_oldFactoryFunc)
            ConnectorFactory::setFactoryFunc(std::move(*m_oldFactoryFunc));

        NX_CRITICAL(DummyConnector::instanceCount == 0);
        NX_CRITICAL(DummyConnection::instanceCount == 0);
    }

    void setConnectorFactoryFunc(ConnectorFactory::FactoryFunc newFactoryFunc)
    {
        auto oldFunc = ConnectorFactory::setFactoryFunc(std::move(newFactoryFunc));
        if (!m_oldFactoryFunc)
            m_oldFactoryFunc = std::move(oldFunc);
    }

private:
    boost::optional<ConnectorFactory::FactoryFunc> m_oldFactoryFunc;
};


TEST_F(OutgoingTunnel, general)
{
    for (int i = 0; i < 100; ++i)
    {
        const bool connectorWillSucceed = nx::utils::random::number(0, 6) > 0;
        const bool connectionWillSucceed = nx::utils::random::number(0, 4) > 0;
        const int connectionsToCreate = nx::utils::random::number(1, 10);
        const bool singleShotConnection = (bool)nx::utils::random::number(0, 1);

        AddressEntry addressEntry("nx_test.com:12345");
        cloud::OutgoingTunnel tunnel(std::move(addressEntry));

        setConnectorFactoryFunc(
            [connectorWillSucceed, connectionWillSucceed, singleShotConnection](
                const AddressEntry& targetAddress) -> std::unique_ptr<AbstractCrossNatConnector>
            {
                return std::make_unique<DummyConnector>(
                    targetAddress,
                    connectorWillSucceed,
                    connectionWillSucceed,
                    singleShotConnection);
            });
    
        for (int i = 0; i < connectionsToCreate; ++i)
        {
            ConnectionCompletedPromise connectedPromise;
            tunnel.establishNewConnection(
                SocketAttributes(),
                [&connectedPromise](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> socket)
                {
                    connectedPromise.set_value(std::make_pair(errorCode, std::move(socket)));
                });

            const auto result = connectedPromise.get_future().get();

            if (connectorWillSucceed && connectionWillSucceed)
            {
                if (singleShotConnection)
                {
                    if (i == 0)
                        ASSERT_EQ(SystemError::noError, result.first);
                    else // i > 0
                        ASSERT_EQ(SystemError::connectionReset, result.first);  //tunnel signals broken connection
                }
                else
                {
                    ASSERT_EQ(SystemError::noError, result.first);
                    ASSERT_NE(nullptr, result.second);
                }
            }
            else
            {
                if (!connectorWillSucceed)
                    ASSERT_EQ(
                        SystemError::connectionRefused,
                        result.first);
                else if (!connectionWillSucceed)
                    ASSERT_EQ(SystemError::connectionReset, result.first);
                ASSERT_EQ(nullptr, result.second);
            }

            if (result.first != SystemError::noError)
            {
                //tunnel should be closed by now, checking it has called "closed" handler
                if (utils::random::number(0, 1))
                    break;
            }
        }

        tunnel.pleaseStopSync();
    }
}

TEST_F(OutgoingTunnel, singleShotConnection)
{
    const bool connectorWillSucceed = false;
    const bool connectionWillSucceed = true;
    const int connectionsToCreate = 2;
    const bool singleShotConnection = false;

    AddressEntry addressEntry("nx_test.com:12345");

    setConnectorFactoryFunc(
        [connectorWillSucceed, connectionWillSucceed, singleShotConnection](
            const AddressEntry& targetAddress)
                -> std::unique_ptr<AbstractCrossNatConnector>
        {
            return std::make_unique<DummyConnector>(
                targetAddress,
                connectorWillSucceed,
                connectionWillSucceed,
                singleShotConnection);
        });

    for (int j = 0; j < 1000; ++j)
    {
        cloud::OutgoingTunnel tunnel(std::move(addressEntry));

        for (int i = 0; i < connectionsToCreate; ++i)
        {
            ConnectionCompletedPromise connectedPromise;
            tunnel.establishNewConnection(
                SocketAttributes(),
                [&connectedPromise](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> socket)
                {
                    connectedPromise.set_value(std::make_pair(errorCode, std::move(socket)));
                });

            const auto result = connectedPromise.get_future().get();

            ASSERT_EQ(SystemError::connectionRefused, result.first);
            ASSERT_EQ(nullptr, result.second);
        }

        tunnel.pleaseStopSync();
    }
}

TEST_F(OutgoingTunnel, handlersQueueingWhileInConnectingState)
{
    const int connectionsToCreate = 100;

    AddressEntry addressEntry("nx_test.com:12345");
    cloud::OutgoingTunnel tunnel(std::move(addressEntry));

    nx::utils::promise<void> doConnectEvent;

    setConnectorFactoryFunc(
        [&doConnectEvent](
            const AddressEntry& targetAddress)
                -> std::unique_ptr<AbstractCrossNatConnector>
        {
            return std::make_unique<DummyConnector>(
                targetAddress,
                &doConnectEvent);
        });

    std::vector<ConnectionCompletedPromise> promises(connectionsToCreate);

    for (int i = 0; i < connectionsToCreate; ++i)
    {
        auto& connectedPromise = promises[i];
        tunnel.establishNewConnection(
            SocketAttributes(),
            [&connectedPromise](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractStreamSocket> socket)
            {
                connectedPromise.set_value(
                    std::make_pair(errorCode, std::move(socket)));
            });
    }

    //signalling acceptor to succeed
    doConnectEvent.set_value();

    //checking all connections have been reported
    for (int i = 0; i < connectionsToCreate; ++i)
    {
        const auto result = promises[i].get_future().get();
        ASSERT_EQ(SystemError::noError, result.first);
        ASSERT_NE(nullptr, result.second);
    }

    tunnel.pleaseStopSync();
}

TEST_F(OutgoingTunnel, cancellation)
{
    const bool connectorWillSucceed = true;
    const bool singleShotConnection = false;
    const bool connectionWillSucceedValues[2] = {true, false};
    const bool waitConnectionCompletionValues[2] = { true, false };

    for (const bool connectionWillSucceed: connectionWillSucceedValues)
        for (const bool waitConnectionCompletion: waitConnectionCompletionValues)
        {
            AddressEntry addressEntry("nx_test.com:12345");
            cloud::OutgoingTunnel tunnel(std::move(addressEntry));

            setConnectorFactoryFunc(
                [connectorWillSucceed, connectionWillSucceed, singleShotConnection](
                    const AddressEntry& targetAddress)
                        -> std::unique_ptr<AbstractCrossNatConnector>
                {
                    return std::make_unique<DummyConnector>(
                        targetAddress,
                        connectorWillSucceed,
                        connectionWillSucceed,
                        singleShotConnection);
                });

            ConnectionCompletedPromise connectedPromise;
            tunnel.establishNewConnection(
                SocketAttributes(),
                [&connectedPromise](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> socket)
                {
                    connectedPromise.set_value(std::make_pair(errorCode, std::move(socket)));
                });

            if (waitConnectionCompletion)
                connectedPromise.get_future().get();

            nx::utils::promise<void> tunnelStoppedPromise;
            tunnel.pleaseStop(
                [&tunnelStoppedPromise]()
                {
                    tunnelStoppedPromise.set_value();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                });
            tunnelStoppedPromise.get_future().wait();

            if (!waitConnectionCompletion)
                ASSERT_TRUE(connectedPromise.get_future().valid());
        }
}

TEST_F(OutgoingTunnel, connectTimeout)
{
    const auto connectorTimeout = std::chrono::seconds(7);
    const int connectionsToCreate = 70;

    for (int i = 0; i < 5; ++i)
    {
        nx::utils::promise<void> doConnectEvent;

        setConnectorFactoryFunc(
            [connectorTimeout, &doConnectEvent](const AddressEntry& targetAddress)
                -> std::unique_ptr<AbstractCrossNatConnector>
            {
                return std::make_unique<DummyConnector>(
                    targetAddress,
                    connectorTimeout,
                    utils::random::number(0, 1) ? &doConnectEvent : nullptr);
            });

        const std::chrono::milliseconds timeout(utils::random::number(1, 2000));
        const std::chrono::milliseconds minTimeoutCorrection(500);
        #ifdef _DEBUG
            const std::chrono::milliseconds timeoutCorrection =
                (timeout / 5) > minTimeoutCorrection
                ? (timeout / 5)
                : minTimeoutCorrection;
        #endif // _DEBUG

        std::vector<ConnectionContext> connectedPromises;
        connectedPromises.resize(connectionsToCreate);

        AddressEntry addressEntry("nx_test.com:12345");
        cloud::OutgoingTunnel tunnel(std::move(addressEntry));
        auto tunnelGuard = makeScopedGuard([&tunnel] { tunnel.pleaseStopSync(); });

        for (auto& connectionContext: connectedPromises)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(utils::random::number(0, 0xFFFF)));
            connectionContext.startTime = std::chrono::steady_clock::now();
            tunnel.establishNewConnection(
                timeout,
                SocketAttributes(),
                [&connectionContext](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> socket)
                {
                    connectionContext.endTime = std::chrono::steady_clock::now();
                    connectionContext.completionPromise.set_value(
                        std::make_pair(errorCode, std::move(socket)));
                });
        }

        //signalling connector to succeed
        doConnectEvent.set_value();

        for (auto& connectionContext : connectedPromises)
        {
            const auto result = connectionContext.completionPromise.get_future().get();
            ASSERT_EQ(SystemError::timedOut, result.first);
            ASSERT_EQ(nullptr, result.second);

            #ifdef _DEBUG
                if (!utils::TestOptions::areTimeAssertsDisabled())
                {
                    const auto actualTimeout =
                        connectionContext.endTime - connectionContext.startTime;

                    EXPECT_GT(actualTimeout, timeout - timeoutCorrection);
                    EXPECT_LT(actualTimeout, timeout + timeoutCorrection);
                }
            #endif
        }
    }
}

/** testing that tunnel passes correct connect timeout to tunnel connection */
TEST_F(OutgoingTunnel, connectTimeout2)
{
    AddressEntry addressEntry("nx_test.com:12345");
    cloud::OutgoingTunnel tunnel(std::move(addressEntry));

    nx::utils::promise<std::chrono::milliseconds> tunnelConnectionInvokedPromise;

    setConnectorFactoryFunc(
        [/*connectorTimeout,*/ &tunnelConnectionInvokedPromise](
            const AddressEntry& targetAddress) -> std::unique_ptr<AbstractCrossNatConnector>
        {
            auto connector = 
                std::make_unique<DummyConnector>(
                    targetAddress,
                    /*connectorTimeout*/ nullptr);
            connector->setConnectionInvokedPromise(&tunnelConnectionInvokedPromise);
            return std::move(connector);
        });

    const std::chrono::seconds connectionTimeout(3);

    ConnectionContext connectionContext;
    connectionContext.startTime = std::chrono::steady_clock::now();
    tunnel.establishNewConnection(
        connectionTimeout,
        SocketAttributes(),
        [&connectionContext](
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractStreamSocket> socket)
        {
            connectionContext.endTime = std::chrono::steady_clock::now();
            connectionContext.completionPromise.set_value(
                std::make_pair(errorCode, std::move(socket)));
        });

    ASSERT_EQ(
        std::future_status::ready,
        connectionContext.completionPromise.get_future().wait_for(connectionTimeout * 2));

    //waiting for DummyConnection::establishNewConnection call and checking timeout
    auto tunnelConnectionInvokedFuture = tunnelConnectionInvokedPromise.get_future();
    ASSERT_EQ(
        std::future_status::ready,
        tunnelConnectionInvokedFuture.wait_for(std::chrono::milliseconds::zero()));
    const auto timeout = tunnelConnectionInvokedFuture.get();
    ASSERT_LE(timeout, connectionTimeout);
    ASSERT_GE(
        timeout,
        connectionTimeout - (connectionContext.endTime - connectionContext.startTime));

    tunnel.pleaseStopSync();
}

TEST_F(OutgoingTunnel, pool)
{
    OutgoingTunnelPool tunnelPool;

    AddressEntry addressEntry("nx_test.com:12345");

    nx::utils::promise<std::chrono::milliseconds> tunnelConnectionInvokedPromise;

    setConnectorFactoryFunc(
        [/*connectorTimeout,*/ &tunnelConnectionInvokedPromise](
            const AddressEntry& targetAddress) -> std::unique_ptr<AbstractCrossNatConnector>
        {
            return std::make_unique<DummyConnector>(
                targetAddress,
                false,
                false,
                false);
        });

    const size_t connectionCount = 1000;
    std::vector<ConnectionContext> connectionContexts(connectionCount);
    for (auto& connectionContext: connectionContexts)
    {
        tunnelPool.establishNewConnection(
            addressEntry,
            std::chrono::milliseconds::zero(),
            SocketAttributes(),
            [&connectionContext](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractStreamSocket> socket)
            {
                connectionContext.completionPromise.set_value(
                    std::make_pair(errorCode, std::move(socket)));
            });
    }

    for (auto& connectionContext : connectionContexts)
    {
        const auto result = connectionContext.completionPromise.get_future().get();
        ASSERT_TRUE(
            result.first == SystemError::connectionRefused ||
            result.first == SystemError::interrupted);
        ASSERT_EQ(nullptr, result.second);
    }

    tunnelPool.pleaseStopSync();
}

class OutgoingTunnelCancellation:
    public OutgoingTunnel
{
public:
    OutgoingTunnelCancellation():
        m_connection(nullptr)
    {
        setConnectorFactoryFunc(
            [](const AddressEntry& targetAddress) -> std::unique_ptr<AbstractCrossNatConnector>
            {
                auto connector = std::make_unique<DummyConnector>(
                    targetAddress,
                    true,
                    true,
                    false);
                return std::move(connector);
            });
    }

protected:
    void givenOpenedTunnel()
    {
        m_tunnel = openTunnel();

        {
            QnMutexLocker lock(&DummyConnection::instanceSetMutex);
            ASSERT_EQ(1U, DummyConnection::instances.size());
            m_connection = *DummyConnection::instances.begin();
        }

        m_connection->ignoreConnectRequests();

        m_tunnel->setOnClosedHandler(
            [this]()
            {
                // Tunnel supports removal within "on close" handler, so doing it.
                m_tunnel.reset();
                m_tunnelHasBeenClosed.set_value();
            });
    }

    void requestNewConnectionFromTunnel()
    {
        m_tunnel->establishNewConnection(
            SocketAttributes(),
            [](SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>) {});
    }

    void whenTunnelConnectionHasBeenClosed()
    {
        m_connection->reportClosure(SystemError::connectionReset);
    }

    void thenTunnelShouldStopCorrectly()
    {
        m_tunnelHasBeenClosed.get_future().wait();
        ASSERT_EQ(nullptr, m_tunnel.get());
    }

private:
    std::unique_ptr<cloud::OutgoingTunnel> m_tunnel;
    nx::utils::promise<void> m_tunnelHasBeenClosed;
    DummyConnection* m_connection;

    std::unique_ptr<cloud::OutgoingTunnel> openTunnel()
    {
        auto tunnel = std::make_unique<cloud::OutgoingTunnel>(AddressEntry("example.com:80"));
        nx::utils::promise<void> tunnelOpened;
        tunnel->establishNewConnection(
            SocketAttributes(),
            [&tunnelOpened](SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>)
            {
                tunnelOpened.set_value();
            });
        tunnelOpened.get_future().wait();

        return tunnel;
    }
};

TEST_F(OutgoingTunnelCancellation, cancellation2)
{
    givenOpenedTunnel();
    requestNewConnectionFromTunnel();
    whenTunnelConnectionHasBeenClosed();
    thenTunnelShouldStopCorrectly();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
