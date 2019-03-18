#include <array>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <nx/casssandra/async_cassandra_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relay/model/remote_relay_peer_pool.h>
#include <nx/cloud/relay/settings.h>

#include "basic_component_test.h"
#include "cassandra_connection_stub.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {


namespace {

class ClusterDbMapRemoteRelayPeerPool:
    public model::ClusterDbMapRemoteRelayPeerPool
{
    using base_type = model::ClusterDbMapRemoteRelayPeerPool;

public:
    ClusterDbMapRemoteRelayPeerPool(
        const conf::Settings& settings,
        nx::utils::SyncQueue<bool>* initEvent)
        :
        base_type(settings),
        m_initEvent(initEvent)
    {
    }

    virtual bool connectToDb() override
    {
        bool connected = base_type::connectToDb();
        m_initEvent->push(connected);
        return connected;
    }

private:
    nx::utils::SyncQueue<bool>* m_initEvent;
};

} // namespace

class RelayService:
    public ::testing::Test,
    public BasicComponentTest
{
public:
    RelayService():
        BasicComponentTest(Mode::singleRelay)
    {
    }

    ~RelayService()
    {
        stopAllInstances();

        if (m_factoryFunctionBak)
        {
            model::RemoteRelayPeerPoolFactory::instance().setCustomFunc(
                std::move(*m_factoryFunctionBak));
        }
    }

protected:
    void givenRelayThatFailedToConnectToDb()
    {
        using namespace std::placeholders;

        m_factoryFunctionBak = model::RemoteRelayPeerPoolFactory::instance().setCustomFunc(
            std::bind(&RelayService::createTestClusterDbMapRemoteRelayPeerPool, this, _1));

        addRelayInstance({}, false);

        // In the case of clusterdb map, database initialization should not fail unless there is
        // sqllite file problem. There is no host to connect to.
    }

    void givenStartedRelay(std::vector<const char*> args = {})
    {
        addRelayInstance(args);
    }

    void whenStartDb()
    {
        // Do nothing.
    }

    void thenRelayHasConnectedToDb()
    {
        while (!m_initEvents.pop()) {}
    }

    void andRelayHasStarted()
    {
        ASSERT_TRUE(relay().waitUntilStarted());
    }

    void thenRelayCanStillBeStopped()
    {
        relay().stop();
    }

private:
    nx::utils::SyncQueue<bool> m_initEvents;
    std::optional<model::RemoteRelayPeerPoolFactory::Function> m_factoryFunctionBak;
    std::unique_ptr<model::AbstractRemoteRelayPeerPool> createTestClusterDbMapRemoteRelayPeerPool(
        const conf::Settings& settings)
    {
        return std::make_unique<ClusterDbMapRemoteRelayPeerPool>(
            settings,
            &m_initEvents);
    }
};

// cassandra specific functionality that doesn't apply to sync engine
TEST_F(RelayService, waits_for_db_availability_if_db_host_is_specified)
{
    givenRelayThatFailedToConnectToDb();

    whenStartDb();

    thenRelayHasConnectedToDb();
    andRelayHasStarted();
}

// cassandra specific functionality that doesn't apply to sync engine
TEST_F(RelayService, can_be_stopped_regardless_of_db_host_availability)
{
    givenRelayThatFailedToConnectToDb();
    thenRelayCanStillBeStopped();
}

//-------------------------------------------------------------------------------------------------

class RelayServiceHttp:
    public RelayService
{
protected:
    void givenTcpConnectionToRelayHttpPort()
    {
        m_connection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(m_connection->connect(
            nx::network::SocketAddress(
                nx::network::HostAddress::localhost,
                relay().moduleInstance()->httpEndpoints().front().port),
            nx::network::kNoTimeout)) << SystemError::getLastOSErrorText().toStdString();
    }

    void assertConnectionClosedByRelay()
    {
        char buf[1024];
        const int result = m_connection->recv(buf, sizeof(buf), 0);
        ASSERT_TRUE(result == 0 || result == -1) << "Unexpected result: " << result;
    }

private:
    std::unique_ptr<nx::network::TCPSocket> m_connection;
};

TEST_F(RelayServiceHttp, connection_inactivity_timeout_present)
{
    givenStartedRelay({"--http/connectionInactivityTimeout=1ms"});
    givenTcpConnectionToRelayHttpPort();

    assertConnectionClosedByRelay();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
