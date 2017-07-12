#include <memory>
#include <gtest/gtest.h>
#include <model/remote_relay_peer_pool.h>
#include <nx/casssandra/async_cassandra_connection.h>
#include "options.h"


namespace nx {
namespace cloud {
namespace relay {
namespace model {

namespace test {

class TestRelayPool: public RemoteRelayPeerPool
{
public:
    using RemoteRelayPeerPool::RemoteRelayPeerPool;
    cassandra::AsyncConnection* getConnection()
    {
        return RemoteRelayPeerPool::getConnection();
    }
};

class RemoteRelayPeerPool: public ::testing::Test
{
protected:
    virtual void TearDown() override
    {
        m_relayPool->getConnection()->executeUpdate("DROP KEYSPACE cdb;").wait();
    }

    void givenDbWithNotExistentCdbKeyspace() {}

    void givenDbWithExistentCdbKeyspace()
    {
        cassandra::AsyncConnection connection(options()->host.c_str());
        connection.executeUpdate("CREATE KEYSPACE cdb;").wait();
    }

    void whenRelayPoolObjectHasBeenCreated()
    {
        m_relayPool.reset(new TestRelayPool(options()->host.c_str()));
    }

    void assertSelectFromTablesResult(
        std::pair<CassError, cassandra::QueryResult>* selectResult)
    {
        ASSERT_EQ(CASS_OK, selectResult->first);
        ASSERT_TRUE(selectResult->second.next());

        std::string tableName;
        ASSERT_TRUE(selectResult->second.get(std::string("table_name"), &tableName));
        ASSERT_EQ("relay_peers", tableName);
    }

    void thenKeyspaceAndTableShouldBeCreated()
    {
        m_relayPool->getConnection()->executeSelect(
            "SELECT table_name FROM system_schema.tables WHERE keyspace_name = 'cdb';")
            .then(
                [this](cf::future<std::pair<CassError, cassandra::QueryResult>> selectFuture)
                {
                    auto selectResult = selectFuture.get();
                    assertSelectFromTablesResult(&selectResult);
                    return cf::unit();
                }).wait();
    }

private:
    std::unique_ptr<TestRelayPool> m_relayPool;
};

TEST_F(RemoteRelayPeerPool, CreateDbStructure_NeededStructureDoesNotExist)
{
    givenDbWithNotExistentCdbKeyspace();
    whenRelayPoolObjectHasBeenCreated();

    thenKeyspaceAndTableShouldBeCreated();
}


TEST_F(RemoteRelayPeerPool, CreateDbStructure_KeyspaceAlreadyExists)
{
    givenDbWithExistentCdbKeyspace();
    whenRelayPoolObjectHasBeenCreated();

    thenKeyspaceAndTableShouldBeCreated();
}


} // namespace test
} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
