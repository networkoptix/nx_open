/**********************************************************
* Feb 4, 2016
* akolesnikov
***********************************************************/

#include <atomic>

#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/future.h>


namespace nx {
namespace network {
namespace cloud {

class DummyConnection
:
    public AbstractOutgoingTunnelConnection
{
public:
    static std::atomic<size_t> instanceCount;

    DummyConnection(
        const String& /*remotePeerId*/,
        bool connectionShouldWorkFine,
        bool singleShot,
        nx::utils::promise<std::chrono::milliseconds>* tunnelConnectionInvokedPromise = nullptr)
    :
        m_connectionShouldWorkFine(connectionShouldWorkFine),
        m_singleShot(singleShot),
        m_tunnelConnectionInvokedPromise(tunnelConnectionInvokedPromise)
    {
        ++instanceCount;
    }

    ~DummyConnection()
    {
        --instanceCount;
    }

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override
    {
        completionHandler();
    }

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes /*socketAttributes*/,
        OnNewConnectionHandler handler) override
    {
        if (m_tunnelConnectionInvokedPromise)
        {
            m_tunnelConnectionInvokedPromise->set_value(timeout);
            m_tunnelConnectionInvokedPromise = nullptr;
        }

        m_aioThreadBinder.post(
            [this, handler]()
            {
                if (m_connectionShouldWorkFine)
                {
                    handler(SystemError::noError, std::make_unique<TCPSocket>(), !m_singleShot);
                    if (m_singleShot)
                        m_connectionShouldWorkFine = false;
                }
                else
                {
                    handler(SystemError::connectionReset, nullptr, false);
                }
            });
    }

private:
    UDPSocket m_aioThreadBinder;
    bool m_connectionShouldWorkFine;
    const bool m_singleShot;
    nx::utils::promise<std::chrono::milliseconds>* m_tunnelConnectionInvokedPromise;
};

std::atomic<size_t> DummyConnection::instanceCount(0);


class DummyConnector
:
    public AbstractTunnelConnector
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
        m_tunnelConnectionInvokedPromise(nullptr)
    {
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
        --instanceCount;
    }

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override
    {
        m_aioThreadBinder.pleaseStop(std::move(completionHandler));
    }

    virtual aio::AbstractAioThread* getAioThread() const override
    {
        return m_aioThreadBinder.getAioThread();
    }
    
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        m_aioThreadBinder.bindToAioThread(aioThread);
    }
    
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override
    {
        m_aioThreadBinder.post(std::move(func));
    }
    
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override
    {
        m_aioThreadBinder.dispatch(std::move(func));
    }

    virtual int getPriority() const override
    {
        return 0;
    }
    virtual void connect(
        std::chrono::milliseconds /*timeout*/,
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractOutgoingTunnelConnection>)> handler) override
    {
        if (m_connectTimeout)
        {
            m_aioThreadBinder.registerTimer(
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
    virtual const AddressEntry& targetPeerAddress() const override
    {
        return m_targetPeerAddress;
    }

    void setConnectionInvokedPromise(
        nx::utils::promise<std::chrono::milliseconds>* const tunnelConnectionInvokedPromise)
    {
        m_tunnelConnectionInvokedPromise = tunnelConnectionInvokedPromise;
    }

private:
    AddressEntry m_targetPeerAddress;
    UDPSocket m_aioThreadBinder;
    const bool m_connectSuccessfully;
    const bool m_connectionShouldWorkFine;
    const bool m_singleShotConnection;
    nx::utils::promise<void>* const m_canSucceedEvent;
    boost::optional<std::chrono::milliseconds> m_connectTimeout;
    nx::utils::promise<std::chrono::milliseconds>* m_tunnelConnectionInvokedPromise;

    void connectInternal(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractOutgoingTunnelConnection>)> handler)
    {
        m_aioThreadBinder.post(
            [this, handler = move(handler)]()
            {
                if (m_canSucceedEvent)
                    m_canSucceedEvent->get_future().wait();

                if (m_connectSuccessfully)
                    handler(
                        SystemError::noError,
                        std::make_unique<DummyConnection>(
                            m_targetPeerAddress.host.toString().toLatin1(),
                            m_connectionShouldWorkFine,
                            m_singleShotConnection,
                            m_tunnelConnectionInvokedPromise));
                else
                    handler(
                        SystemError::connectionRefused,
                        std::unique_ptr<DummyConnection>());
            });
    }
};

std::atomic<size_t> DummyConnector::instanceCount(0);


class OutgoingTunnelTest
:
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

    OutgoingTunnelTest()
    {
        NX_CRITICAL(DummyConnector::instanceCount == 0);
        NX_CRITICAL(DummyConnection::instanceCount == 0);
    }

    ~OutgoingTunnelTest()
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


TEST_F(OutgoingTunnelTest, general)
{
    for (int i = 0; i < 100; ++i)
    {
        const bool connectorWillSucceed = (rand() % 7) > 0;
        const bool connectionWillSucceed = (rand() % 5) > 0;
        const int connectionsToCreate = (rand() % 10) + 1;
        const bool singleShotConnection = rand() & 1;

        AddressEntry addressEntry("nx_test.com:12345");
        OutgoingTunnel tunnel(std::move(addressEntry));

        setConnectorFactoryFunc(
            [connectorWillSucceed, connectionWillSucceed, singleShotConnection](
                const AddressEntry& targetAddress) -> ConnectorFactory::CloudConnectors
            {
                ConnectorFactory::CloudConnectors connectors;
                connectors.emplace(
                    CloudConnectType::kUdtHp,
                    std::make_unique<DummyConnector>(
                        targetAddress,
                        connectorWillSucceed,
                        connectionWillSucceed,
                        singleShotConnection));
                return connectors;
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
                if (rand() & 1)
                    break;
            }
        }

        tunnel.pleaseStopSync();
    }
}

TEST_F(OutgoingTunnelTest, singleShotConnection)
{
    const bool connectorWillSucceed = false;
    const bool connectionWillSucceed = true;
    const int connectionsToCreate = 2;
    const bool singleShotConnection = false;

    AddressEntry addressEntry("nx_test.com:12345");

    setConnectorFactoryFunc(
        [connectorWillSucceed, connectionWillSucceed, singleShotConnection](
            const AddressEntry& targetAddress) -> ConnectorFactory::CloudConnectors
        {
            ConnectorFactory::CloudConnectors connectors;
            connectors.emplace(
                CloudConnectType::kUdtHp,
                std::make_unique<DummyConnector>(
                    targetAddress,
                    connectorWillSucceed,
                    connectionWillSucceed,
                    singleShotConnection));
            return connectors;
        });

    for (int j = 0; j < 1000; ++j)
    {
        OutgoingTunnel tunnel(std::move(addressEntry));

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

TEST_F(OutgoingTunnelTest, handlersQueueingWhileInConnectingState)
{
    const int connectionsToCreate = 100;

    AddressEntry addressEntry("nx_test.com:12345");
    OutgoingTunnel tunnel(std::move(addressEntry));

    nx::utils::promise<void> doConnectEvent;

    setConnectorFactoryFunc(
        [&doConnectEvent](
            const AddressEntry& targetAddress) -> ConnectorFactory::CloudConnectors
        {
            ConnectorFactory::CloudConnectors connectors;
            connectors.emplace(
                CloudConnectType::kUdtHp,
                std::make_unique<DummyConnector>(
                    targetAddress,
                    &doConnectEvent));
            return connectors;
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

TEST_F(OutgoingTunnelTest, cancellation)
{
    const bool connectorWillSucceed = true;
    const bool singleShotConnection = false;
    const bool connectionWillSucceedValues[2] = {true, false};
    const bool waitConnectionCompletionValues[2] = { true, false };

    for (const bool connectionWillSucceed: connectionWillSucceedValues)
        for (const bool waitConnectionCompletion: waitConnectionCompletionValues)
        {
            AddressEntry addressEntry("nx_test.com:12345");
            OutgoingTunnel tunnel(std::move(addressEntry));

            setConnectorFactoryFunc(
                [connectorWillSucceed, connectionWillSucceed, singleShotConnection](
                    const AddressEntry& targetAddress) -> ConnectorFactory::CloudConnectors
                {
                    ConnectorFactory::CloudConnectors connectors;
                    connectors.emplace(
                        CloudConnectType::kUdtHp,
                        std::make_unique<DummyConnector>(
                            targetAddress,
                            connectorWillSucceed,
                            connectionWillSucceed,
                            singleShotConnection));
                    return connectors;
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

TEST_F(OutgoingTunnelTest, connectTimeout)
{
    const bool connectorWillSucceed = true;
    const bool singleShotConnection = false;
    const bool connectionWillSucceedValues[1] = { true };
    const auto connectorTimeout = std::chrono::seconds(7);
    const int connectionsToCreate = 70;

    for (int i = 0; i < 5; ++i)
        for (const bool connectionWillSucceed : connectionWillSucceedValues)
        {
            AddressEntry addressEntry("nx_test.com:12345");
            OutgoingTunnel tunnel(std::move(addressEntry));

            nx::utils::promise<void> doConnectEvent;

            setConnectorFactoryFunc(
                [connectorTimeout, &doConnectEvent](const AddressEntry& targetAddress) ->
                    ConnectorFactory::CloudConnectors
                {
                    ConnectorFactory::CloudConnectors connectors;
                    connectors.emplace(
                        CloudConnectType::kUdtHp,
                        std::make_unique<DummyConnector>(
                            targetAddress,
                            connectorTimeout,
                            (rand() & 1) ? &doConnectEvent : nullptr));
                    return connectors;
                });

            const std::chrono::milliseconds timeout(1 + (rand() % 2000));
            const std::chrono::milliseconds minTimeoutCorrection(500);
            const std::chrono::milliseconds timeoutCorrection =
                (timeout / 5) > minTimeoutCorrection
                ? (timeout / 5)
                : minTimeoutCorrection;

            std::vector<ConnectionContext> connectedPromises;
            connectedPromises.resize(connectionsToCreate);
            for (auto& connectionContext: connectedPromises)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(rand() & 0xffff));
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
                const auto actualTimeout = connectionContext.endTime - connectionContext.startTime;

                ASSERT_EQ(SystemError::timedOut, result.first);
                ASSERT_EQ(nullptr, result.second);
                ASSERT_TRUE(
                    (actualTimeout > (timeout - timeoutCorrection)) &&
                    (actualTimeout < (timeout + timeoutCorrection)));
            }

            tunnel.pleaseStopSync();
        }
}

/** testing that tunnel passes correct connect timeout to tunnel connection */
TEST_F(OutgoingTunnelTest, connectTimeout2)
{
    const bool connectorWillSucceed = true;
    const bool singleShotConnection = false;
    const bool connectionWillSucceedValues[1] = { true };
    //const auto connectorTimeout = std::chrono::seconds(3);

    AddressEntry addressEntry("nx_test.com:12345");
    OutgoingTunnel tunnel(std::move(addressEntry));

    nx::utils::promise<std::chrono::milliseconds> tunnelConnectionInvokedPromise;

    setConnectorFactoryFunc(
        [/*connectorTimeout,*/ &tunnelConnectionInvokedPromise](
            const AddressEntry& targetAddress) -> ConnectorFactory::CloudConnectors
        {
            ConnectorFactory::CloudConnectors connectors;
            auto connector = 
                std::make_unique<DummyConnector>(
                    targetAddress,
                    /*connectorTimeout*/ nullptr);
            connector->setConnectionInvokedPromise(&tunnelConnectionInvokedPromise);
            connectors.emplace(
                CloudConnectType::kUdtHp,
                std::move(connector));
            return connectors;
        });

    const std::chrono::seconds connectionTimeout(3);

    ConnectionContext connectionContext;
    //std::this_thread::sleep_for(std::chrono::microseconds(rand() & 0xffff));
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

TEST_F(OutgoingTunnelTest, pool)
{
    OutgoingTunnelPool tunnelPool;

    AddressEntry addressEntry("nx_test.com:12345");

    nx::utils::promise<std::chrono::milliseconds> tunnelConnectionInvokedPromise;

    setConnectorFactoryFunc(
        [/*connectorTimeout,*/ &tunnelConnectionInvokedPromise](
            const AddressEntry& targetAddress) -> ConnectorFactory::CloudConnectors
        {
            ConnectorFactory::CloudConnectors connectors;
            connectors.emplace(
                CloudConnectType::kUdtHp,
                std::make_unique<DummyConnector>(
                    targetAddress,
                    false,
                    false,
                    false));
            return connectors;
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

} // namespace cloud
} // namespace network
} // namespace nx
