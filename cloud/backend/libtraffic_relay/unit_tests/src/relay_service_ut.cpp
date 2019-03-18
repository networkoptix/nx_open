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

class TestRemoteRelayPeerPool:
    public model::RemoteRelayPeerPool
{
    using base_type = model::RemoteRelayPeerPool;

public:
    TestRemoteRelayPeerPool(
        const conf::Settings& settings,
        nx::utils::SyncQueue<bool>* initEvents,
        const bool* const forceConnectionFailure)
        :
        base_type(settings),
        m_initEvents(initEvents),
        m_forceConnectionFailure(forceConnectionFailure)
    {
    }

    virtual bool connectToDb() override
    {
        if (*m_forceConnectionFailure)
        {
            m_initEvents->push(false);
            return false;
        }

        bool connected = base_type::connectToDb();
        m_initEvents->push(connected);
        return connected;
    }

    virtual bool isConnected() const override
    {
        if (*m_forceConnectionFailure)
            return false;

        return base_type::isConnected();
    }

private:
    nx::utils::SyncQueue<bool>* m_initEvents;
    const bool* const m_forceConnectionFailure = nullptr;
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

        m_forceConnectionFailure = true;

        m_factoryFunctionBak = model::RemoteRelayPeerPoolFactory::instance().setCustomFunc(
            [this](const conf::Settings& settings)
            {
                return std::make_unique<TestRemoteRelayPeerPool>(
                    settings,
                    &m_initEvents,
                    &m_forceConnectionFailure);
            });

        addRelayInstance({}, false);

        ASSERT_FALSE(m_initEvents.pop());
    }

    void givenStartedRelay(std::vector<const char*> args = {})
    {
        addRelayInstance(args);
    }

    void whenStartDb()
    {
        m_forceConnectionFailure = false;
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
    bool m_forceConnectionFailure = false;
    nx::utils::SyncQueue<bool> m_initEvents;
    std::optional<model::RemoteRelayPeerPoolFactory::Function> m_factoryFunctionBak;
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
