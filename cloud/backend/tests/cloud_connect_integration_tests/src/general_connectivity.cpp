#include <tuple>

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/cloud/tunnel/cross_nat_connector.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/sync_queue.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class GeneralConnectivity:
    public BasicTestFixture,
    public ::testing::Test
{
    using base_type = BasicTestFixture;

protected:
    void givenListeningCloudServer()
    {
        assertConnectionCanBeEstablished();
    }

    void whenConnectToThePeerUsingDomainName()
    {
        setRemotePeerName(cloudSystemCredentials().systemId.toStdString());
        assertConnectionCanBeEstablished();
    }

    void whenConnectionToMediatorIsBroken()
    {
        restartMediator();
    }

    void thenClientSocketProvidesRemotePeerFullName()
    {
        auto cloudStreamSocket = dynamic_cast<const CloudStreamSocket*>(clientSocket().get());
        ASSERT_NE(nullptr, cloudStreamSocket);
        ASSERT_EQ(
            cloudSystemCredentials().hostName(),
            cloudStreamSocket->getForeignHostName());
    }

    void thenConnectionToMediatorIsRestored()
    {
        waitUntilServerIsRegisteredOnMediator();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        startServer();
    }
};

TEST_F(GeneralConnectivity, connect_works)
{
    assertConnectionCanBeEstablished();
}

TEST_F(GeneralConnectivity, remote_peer_full_name_is_known_on_client_side)
{
    whenConnectToThePeerUsingDomainName();
    thenClientSocketProvidesRemotePeerFullName();
}

TEST_F(GeneralConnectivity, server_accepts_connections_after_reconnect_to_mediator)
{
    givenListeningCloudServer();

    whenConnectionToMediatorIsBroken();

    thenConnectionToMediatorIsRestored();
    assertConnectionCanBeEstablished();
}

//-------------------------------------------------------------------------------------------------

class GeneralConnectivityCompatibilityRawStunConnection:
    public GeneralConnectivity
{
    using base_type = GeneralConnectivity;

public:
    GeneralConnectivityCompatibilityRawStunConnection()
    {
        setMediatorApiProtocol(MediatorApiProtocol::stun);
    }
};

TEST_F(GeneralConnectivityCompatibilityRawStunConnection, cloud_connection_can_be_established)
{
    assertConnectionCanBeEstablished();
}

//-------------------------------------------------------------------------------------------------

class AsyncExecutor
{
public:
    virtual ~AsyncExecutor() = default;

    virtual void setMaxConcurrentOperations(int value) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual int operationsCompletedCount() const = 0;
};

template<typename Operation, typename ...Args>
class AsyncExecutorImpl:
    public AsyncExecutor
{
public:
    AsyncExecutorImpl(Args... args):
        m_args(args...)
    {
    }

    ~AsyncExecutorImpl()
    {
        stop();
    }

    virtual void setMaxConcurrentOperations(int value) override
    {
        QnMutexLocker locker(&m_mutex);
        m_maxConcurrentOperations = value;
    }

    virtual void start() override
    {
        for (int i = 0; i < m_maxConcurrentOperations; ++i)
            startOperation();
    }

    virtual void stop() override
    {
        decltype(m_operations) operations;
        {
            QnMutexLocker lock(&m_mutex);
            operations.swap(m_operations);
            m_stopped = true;
        }

        for (auto& operation: operations)
            operation->pleaseStopSync();
    }

    virtual int operationsCompletedCount() const override
    {
        return m_operationsCompletedCount.load();
    }

private:
    using Operations = std::list<std::unique_ptr<Operation>>;

    mutable QnMutex m_mutex;
    Operations m_operations;
    int m_maxConcurrentOperations = 7;
    bool m_stopped = false;
    std::tuple<Args...> m_args;
    std::atomic<int> m_operationsCompletedCount{0};

    void startOperation()
    {
        QnMutexLocker locker(&m_mutex);
        startOperation(locker);
    }

    void startOperation(const QnMutexLockerBase& /*locker*/)
    {
        auto operation = std::apply(
            [](auto... arg) { return std::make_unique<Operation>(arg...); },
            m_args);
        m_operations.push_back(std::move(operation));
        m_operations.back()->start(std::bind(
            &AsyncExecutorImpl::cleanUpOperation, this, std::prev(m_operations.end())));
    }

    void cleanUpOperation(typename Operations::iterator operationIter)
    {
        QnMutexLocker locker(&m_mutex);

        if (m_stopped)
            return;

        m_operations.erase(operationIter);
        ++m_operationsCompletedCount;

        for (int i = (int)m_operations.size(); i < m_maxConcurrentOperations; ++i)
            startOperation(locker);
    }
};

template<typename Operation, typename ...Args>
std::unique_ptr<AsyncExecutor> makeAsyncExecutor(Args... args)
{
    using Executor = AsyncExecutorImpl<Operation, Args...>;
    return std::make_unique<Executor>(args...);
}

//-------------------------------------------------------------------------------------------------

class CloudConnectivityStressTest:
    public GeneralConnectivity
{
    using base_type = GeneralConnectivity;

public:
    CloudConnectivityStressTest()
    {
        mediator().addArg("--cloudConnect/connectionAckAwaitTimeout=1ms");
    }

    ~CloudConnectivityStressTest()
    {
        if (m_executor)
            m_executor->stop();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();
    }

    void establishMultipleCloudTunnels()
    {
        startMultipleCloudConnections();
        waitForNumberOfCloudConnectionsCompleted();
    }

    void startMultipleCloudConnections()
    {
        m_executor = makeAsyncExecutor<Operation>(
            hpm::api::MediatorAddress{
                mediator().httpUrl(),
                mediator().stunUdpEndpoint()},
            serverSocketCloudAddress());

        m_executor->setMaxConcurrentOperations(27);
        m_executor->start();
    }

    void waitForNumberOfCloudConnectionsCompleted()
    {
        while (m_executor->operationsCompletedCount() < 47)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

private:
    using Result = std::tuple<
        SystemError::ErrorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>>;

    class Operation
    {
    public:
        Operation(
            const hpm::api::MediatorAddress& mediatorAddress,
            const std::string& host)
        {
            AddressEntry addressEntry;
            addressEntry.type = AddressType::cloud;
            addressEntry.host = host.c_str();

            m_crossNatConnector = std::make_unique<CrossNatConnector>(
                &nx::network::SocketGlobals::cloud(),
                addressEntry,
                mediatorAddress);
        }

        void start(nx::utils::MoveOnlyFunc<void()> handler)
        {
            m_crossNatConnector->connect(
                std::chrono::milliseconds::zero(),
                std::bind(std::move(handler)));
        }

        void pleaseStopSync()
        {
            m_crossNatConnector->pleaseStopSync();
        }

    private:
        nx::utils::MoveOnlyFunc<void()> m_handler;
        std::unique_ptr<CrossNatConnector> m_crossNatConnector;
    };

    std::unique_ptr<AsyncExecutor> m_executor;
};

TEST_F(CloudConnectivityStressTest, low_connect_session_timeout_on_mediator)
{
    establishMultipleCloudTunnels();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
