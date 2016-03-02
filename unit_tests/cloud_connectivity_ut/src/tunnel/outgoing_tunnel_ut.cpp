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
        bool singleShot)
    :
        m_connectionShouldWorkFine(connectionShouldWorkFine),
        m_singleShot(singleShot)
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
        std::chrono::milliseconds /*timeout*/,
        SocketAttributes /*socketAttributes*/,
        OnNewConnectionHandler handler) override
    {
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
        bool singleShotConnection)
    :
        m_targetPeerAddress(std::move(targetPeerAddress)),
        m_connectSuccessfully(connectSuccessfully),
        m_connectionShouldWorkFine(connectionShouldWorkFine),
        m_singleShotConnection(singleShotConnection),
        m_canSucceedEvent(nullptr)
    {
        ++instanceCount;
    }

    DummyConnector(
        AddressEntry targetPeerAddress,
        std::promise<void>* const canSucceedEvent)
    :
        m_targetPeerAddress(std::move(targetPeerAddress)),
        m_connectSuccessfully(true),
        m_connectionShouldWorkFine(true),
        m_singleShotConnection(false),
        m_canSucceedEvent(canSucceedEvent)
    {
        ++instanceCount;
    }

    DummyConnector(
        AddressEntry targetPeerAddress,
        std::chrono::milliseconds connectTimeout,
        std::promise<void>* const canSucceedEvent = nullptr)
    :
        m_targetPeerAddress(std::move(targetPeerAddress)),
        m_connectSuccessfully(true),
        m_connectionShouldWorkFine(true),
        m_singleShotConnection(false),
        m_canSucceedEvent(canSucceedEvent),
        m_connectTimeout(std::move(connectTimeout))
    {
        ++instanceCount;
    }

    ~DummyConnector()
    {
        --instanceCount;
    }

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override
    {
        m_aioThreadBinder.pleaseStop(std::move(completionHandler));
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

private:
    AddressEntry m_targetPeerAddress;
    UDPSocket m_aioThreadBinder;
    const bool m_connectSuccessfully;
    const bool m_connectionShouldWorkFine;
    const bool m_singleShotConnection;
    std::promise<void>* const m_canSucceedEvent;
    boost::optional<std::chrono::milliseconds> m_connectTimeout;

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
                            m_singleShotConnection));
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
    typedef std::promise<std::pair<
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
        NX_ASSERT(DummyConnector::instanceCount == 0);
        NX_ASSERT(DummyConnection::instanceCount == 0);
    }

    ~OutgoingTunnelTest()
    {
        if (m_oldFactoryFunc)
            ConnectorFactory::setFactoryFunc(std::move(*m_oldFactoryFunc));

        NX_ASSERT(DummyConnector::instanceCount == 0);
        NX_ASSERT(DummyConnection::instanceCount == 0);
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
    const int testLoopLength = 10000;

    for (int i = 0; i < testLoopLength; ++i)
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
                        i == 0
                            ? SystemError::connectionRefused
                            : SystemError::connectionReset, //tunnel signals broken connection
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

            if (i == 0)
            {
                ASSERT_EQ(SystemError::connectionRefused, result.first);
                ASSERT_EQ(nullptr, result.second);
            }
            else
            {
                ASSERT_EQ(SystemError::connectionReset, result.first);
                ASSERT_EQ(nullptr, result.second);
            }
        }

        tunnel.pleaseStopSync();
    }
}

TEST_F(OutgoingTunnelTest, handlersQueueingWhileInConnectingState)
{
    const int connectionsToCreate = 100;

    AddressEntry addressEntry("nx_test.com:12345");
    OutgoingTunnel tunnel(std::move(addressEntry));

    std::promise<void> doConnectEvent;

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

            std::promise<void> tunnelStoppedPromise;
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

            std::promise<void> doConnectEvent;

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

} // namespace cloud
} // namespace network
} // namespace nx
