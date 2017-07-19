#include <memory>
#include <vector>
#include <utility>
#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>
#include <model/remote_relay_peer_pool.h>
#include <nx/casssandra/async_cassandra_connection.h>
#include <nx/utils/std/algorithm.h>
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
            std::make_pair(
                "68094200-0598-46B9-AE5D-9F08DF45C062",
                "someServer4.subdomain.nx.network.org"));
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

        boost::optional<std::string> tableName;
        ASSERT_TRUE(selectResult->second.get(std::string("table_name"), &tableName));
        ASSERT_TRUE((bool)tableName);
        ASSERT_EQ("relay_peers", *tableName);
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

        boost::optional<std::string> suffix1, suffix2, suffix3, domainTail;
        boost::optional<cassandra::Uuid> relayId;

        int rowCount = 0;
        while (selectResult.second.next())
        {
            ++rowCount;
            ASSERT_TRUE(selectResult.second.get(0, &relayId));
            ASSERT_TRUE(selectResult.second.get(1, &suffix1));
            ASSERT_TRUE(selectResult.second.get(2, &suffix2));
            ASSERT_TRUE(selectResult.second.get(3, &suffix3));
            ASSERT_TRUE(selectResult.second.get(4, &domainTail));

            ASSERT_TRUE((bool)relayId);
            ASSERT_TRUE((bool)suffix1);
            ASSERT_TRUE((bool)suffix2);
            ASSERT_TRUE((bool)suffix3);
            ASSERT_TRUE((bool)domainTail);

            ASSERT_FALSE(relayId->uuidString.empty());

            auto testDataIt = std::find_if(m_relayToDomainTestData.cbegin(),
                m_relayToDomainTestData.cend(),
                [&](const std::pair<std::string, std::string>& p)
                {
                    auto testStringReversed = nx::utils::reverseWords(p.second, ".");
                    std::vector<std::string> reversedParts;
                    boost::split(reversedParts, testStringReversed, boost::is_any_of("."));

                    std::string domainTailString;
                    if (reversedParts.size() >= 4)
                    {
                        for (size_t i = 3; i < reversedParts.size(); ++i)
                        {
                            domainTailString += reversedParts[i];
                            if (i != reversedParts.size() - 1)
                                domainTailString += ".";
                        }
                    }

                    if (reversedParts[0] != *suffix1)
                        return false;

                    if ((reversedParts.size() > 1 && reversedParts[1] != *suffix2)
                        || (reversedParts.size() <= 1 && *suffix2 != ""))
                    {
                        return false;
                    }

                    if ((reversedParts.size() > 2 && reversedParts[2] != *suffix3)
                        || (reversedParts.size() <= 2 && *suffix3 != ""))
                    {
                        return false;
                    }

                    if ((reversedParts.size() > 3 && domainTailString != *domainTail)
                        || (reversedParts.size() <= 3 && *domainTail!= ""))
                    {
                        return false;
                    }

                    return true;
                });

            ASSERT_NE(m_relayToDomainTestData.cend(), testDataIt);
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
