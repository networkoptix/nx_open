#include <array>
#include <memory>
#include <string>

#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <nx/casssandra/async_cassandra_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>
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

class RelayService:
    public ::testing::Test,
    public BasicComponentTest
{
public:
    RelayService()
    {
    }

    ~RelayService()
    {
        stop();

        if (m_cassandraConnectionFactoryBak)
        {
            nx::cassandra::AsyncConnectionFactory::instance().setCustomFunc(
                std::move(*m_cassandraConnectionFactoryBak));
        }
    }

protected:
    void givenRelayThatFailedToConnectToDb()
    {
        using namespace std::placeholders;

        addArg("--cassandra/host=testHost");
        addArg("--cassandra/delayBeforeRetryingInitialConnect=1ms");

        m_isCassandraOnline = false;
        m_cassandraConnectionFactoryBak =
            nx::cassandra::AsyncConnectionFactory::instance().setCustomFunc(
                std::bind(&RelayService::createCassandraConnectionStub, this, _1));

        start();

        // Waiting for cassandra connect to fail.
        ASSERT_FALSE(m_cassandraConnectionInitializationEvents.pop());
    }

    void givenStartedRelay()
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void whenStartDb()
    {
        m_isCassandraOnline = true;
    }

    void thenRelayHasConnectedToDb()
    {
        ASSERT_TRUE(m_cassandraConnectionInitializationEvents.pop());
    }

    void andRelayHasStarted()
    {
        ASSERT_TRUE(waitUntilStarted());
    }

    void thenRelayCanStillBeStopped()
    {
        stop();
    }

private:
    boost::optional<nx::cassandra::AsyncConnectionFactory::Function>
        m_cassandraConnectionFactoryBak;
    bool m_isCassandraOnline = true;
    nx::utils::SyncQueue<bool> m_cassandraConnectionInitializationEvents;

    std::unique_ptr<nx::cassandra::AbstractAsyncConnection>
        createCassandraConnectionStub(const std::string& /*dbHostName*/)
    {
        auto connection = std::make_unique<CassandraConnectionStub>();
        connection->setDbHostAvailable(m_isCassandraOnline);
        connection->setInitializationDoneEventQueue(
            &m_cassandraConnectionInitializationEvents);
        return std::move(connection);
    }
};

TEST_F(RelayService, waits_for_db_availability_if_db_host_is_specified)
{
    givenRelayThatFailedToConnectToDb();

    whenStartDb();

    thenRelayHasConnectedToDb();
    andRelayHasStarted();
}

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
            nx::network::SocketAddress(nx::network::HostAddress::localhost, moduleInstance()->httpEndpoints().front().port),
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
    addArg("--http/connectionInactivityTimeout=1ms");

    givenStartedRelay();
    givenTcpConnectionToRelayHttpPort();

    assertConnectionClosedByRelay();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
