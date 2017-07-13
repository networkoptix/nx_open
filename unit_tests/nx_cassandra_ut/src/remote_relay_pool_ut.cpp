#include <memory>
#include <vector>
#include <utility>
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
        if (m_relayPool)
            m_relayPool->getConnection()->executeUpdate("DROP KEYSPACE cdb;").wait();
    }

    virtual void SetUp() override
    {
        m_relayToDomainTestData.push_back(
            std::make_pair("87C86E7B-3EAE-42F7-97FB-528FC1C0A25F", "someServer.nx.com"));
        m_relayToDomainTestData.push_back(
            std::make_pair("BFF1EA22-56F3-4433-92A3-B412B733282F", "someServer.nx.network.com"));
        m_relayToDomainTestData.push_back(
            std::make_pair("1160E08E-0893-4CDF-8551-F86919FEF853", "someServer2.nx.network.ru"));
        m_relayToDomainTestData.push_back(
            std::make_pair("3CB0A381-A6A1-4905-A1E9-E1CF743A7042", "someServer3.com"));
        m_relayToDomainTestData.push_back(
            std::make_pair("68094200-0598-46B9-AE5D-9F08DF45C062", "someServer4.nx.network.org"));
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

    void whenFivePeersHaveBeenAdded()
    {
        for (const auto& relayToDomain: m_relayToDomainTestData)
            ASSERT_TRUE(m_relayPool->addPeer(relayToDomain.second).get());
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

    void thenAllRecordsExistAndSplitCorreclty()
    {
        auto selectResult = m_relayPool->getConnection()->executeSelect(
            "SELECT * from cdb.relay_peers;").get();

        ASSERT_EQ(CASS_OK, selectResult.first);

        int rowCount = 0;
        while (selectResult.second.next())
        {
            ++rowCount;
        }

        ASSERT_EQ((int)m_relayToDomainTestData.size(), rowCount);
    }

private:
    std::unique_ptr<TestRelayPool> m_relayPool;
    std::vector<std::pair<std::string, std::string>> m_relayToDomainTestData;
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

TEST_F(RemoteRelayPeerPool, addPeer)
{
    givenDbWithNotExistentCdbKeyspace();
    whenRelayPoolObjectHasBeenCreated();

    whenFivePeersHaveBeenAdded();
    thenAllRecordsExistAndSplitCorreclty();
}


} // namespace test
} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
