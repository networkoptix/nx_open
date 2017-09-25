#include <memory>
#include <vector>
#include <utility>
#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>
#include <model/remote_relay_peer_pool.h>
#include <model/../settings.h>
#include <nx/casssandra/async_cassandra_connection.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/thread/cf/wrappers.h>
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
        return nx::cloud::relay::model::RemoteRelayPeerPool::getConnection();
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
            std::make_tuple("87C86E7B-3EAE-42F7-97FB-528FC1C0A25F",
                "relayHost1", "someServer.nx.com"));
        m_relayToDomainTestData.push_back(
            std::make_tuple("BFF1EA22-56F3-4433-92A3-B412B733282F",
                "relayHost2", "someServer.nx.network.com"));
        m_relayToDomainTestData.push_back(
            std::make_tuple("1160E08E-0893-4CDF-8551-F86919FEF853",
                "relayHost3", "someServer2.nx.network.ru"));
        m_relayToDomainTestData.push_back(
            std::make_tuple("3CB0A381-A6A1-4905-A1E9-E1CF743A7042",
                "relayHost4", "someServer3.com"));
        m_relayToDomainTestData.push_back(
            std::make_tuple("68094200-0598-46B9-AE5D-9F08DF45C062",
                "relayHost5", "someServer4.subdomain.nx.network.org"));
    }

    void givenDbWithNotExistentCdbKeyspace() {}

    void givenDbWithExistentCdbKeyspace()
    {
        cassandra::AsyncConnection connection(options()->host.c_str());
        connection.executeUpdate("CREATE KEYSPACE cdb;").wait();
    }

    void whenRelayPoolObjectHasBeenCreated()
    {
        std::vector<const char*> argv;
        argv.push_back("cassandra/host");
        argv.push_back(options()->host.c_str());

        conf::Settings settings;
        settings.load(argv.size(), argv.data());

        m_relayPool.reset(new TestRelayPool(settings));
    }

    void whenFivePeersHaveBeenAdded()
    {
        for (const auto& relayToDomain: m_relayToDomainTestData)
        {
            ASSERT_TRUE(m_relayPool->addPeer(std::get<2>(relayToDomain),
                std::get<1>(relayToDomain)).get());
        }
    }

    void whenFivePeersHaveBeenRemoved()
    {
        for (const auto& relayToDomain : m_relayToDomainTestData)
            ASSERT_TRUE(m_relayPool->removePeer(std::get<2>(relayToDomain)).get());
    }

    void whenTableIsFilledWithTestData()
    {
        struct Context
        {
            size_t index = 0;
        };
        auto sharedContext = std::make_shared<Context>();

        cf::doWhile(
            [this, sharedContext]()
            {
                return m_relayPool->getConnection()->prepareQuery(
                    "INSERT INTO cdb.relay_peers ( \
                                relay_id, \
                                relay_host, \
                                domain_suffix_1, \
                                domain_suffix_2, \
                                domain_suffix_3, \
                                domain_name_tail ) VALUES(?, ?, ?, ?, ?, ?);")
                    .then(
                        [this, sharedContext](
                            cf::future<std::pair<CassError, cassandra::Query>> prepareFuture)
                        {
                            auto prepareResult = prepareFuture.get();
                            NX_ASSERT(prepareResult.first == CASS_OK);

                            cassandra::Query& query = prepareResult.second;
                            const auto& testData = m_relayToDomainTestData[sharedContext->index];
                            NX_ASSERT(query.bind("relay_id",
                                cassandra::Uuid(std::get<0>(testData))));
                            NX_ASSERT(query.bind("relay_host", std::get<1>(testData)));

                            auto splitDomainResult = getDomainParts(std::get<2>(testData));
                            const auto& reversedParts = splitDomainResult.first;
                            const auto& domainTailString = splitDomainResult.second;
                            NX_ASSERT(!reversedParts.empty());

                            NX_ASSERT(query.bind("domain_suffix_1", reversedParts[0]));
                            NX_ASSERT(query.bind(
                                "domain_suffix_2", reversedParts.size() > 1
                                    ? reversedParts[1]
                                    : ""));
                            NX_ASSERT(query.bind(
                                "domain_suffix_3", reversedParts.size() > 2
                                    ? reversedParts[2]
                                    : ""));
                            NX_ASSERT(query.bind("domain_name_tail", domainTailString));

                            return m_relayPool->getConnection()->executeUpdate(std::move(query))
                                .then(
                                    [this, sharedContext](cf::future<CassError> insertFuture)
                                    {
                                        NX_ASSERT(insertFuture.get() == CASS_OK);
                                        return ++sharedContext->index >=
                                            m_relayToDomainTestData.size()
                                                ? false : true;
                                    });
                        });
            }).wait();
    }

    void whenOneRecordIsReplacedWithDifferentRelayHostAndId()
    {
        auto replaceResult = m_relayPool->getConnection()->prepareQuery(
            "INSERT INTO cdb.relay_peers ( \
                relay_id, \
                relay_host, \
                domain_suffix_1, \
                domain_suffix_2, \
                domain_suffix_3, \
                domain_name_tail ) VALUES(?, ?, ?, ?, ?, ?);")
            .then(
                [this](cf::future<std::pair<CassError, cassandra::Query>> prepareFuture)
                {
                    auto prepareResult = prepareFuture.get();
                    NX_ASSERT(prepareResult.first == CASS_OK);

                    auto& query = prepareResult.second;
                    NX_ASSERT(query.bind(
                        "relay_id",
                        cassandra::Uuid("74DFEB4A-1FBD-49AD-B840-5F477A7B4A72")));
                    NX_ASSERT(query.bind("relay_host", "new_host"));
                    NX_ASSERT(query.bind("domain_suffix_1", "com"));
                    NX_ASSERT(query.bind("domain_suffix_2", "nx"));
                    NX_ASSERT(query.bind("domain_suffix_3", "someServer"));
                    NX_ASSERT(query.bind("domain_name_tail", ""));

                    return m_relayPool->getConnection()->executeUpdate(std::move(query));
                }).get();

        NX_ASSERT(replaceResult == CASS_OK);
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

    std::pair<std::vector<std::string>, std::string> getDomainParts(const std::string& domainName)
    {
        auto domainNameReversed = nx::utils::reverseWords(domainName, ".");
        std::vector<std::string> reversedParts;
        boost::split(reversedParts, domainNameReversed, boost::is_any_of("."));

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

        return std::make_pair(reversedParts, domainTailString);
    }

    void thenRelayPeersTableShouldBeEmpty()
    {
        auto selectResult = m_relayPool->getConnection()->executeSelect(
            "SELECT * from cdb.relay_peers;").get();

        ASSERT_EQ(CASS_OK, selectResult.first);
        ASSERT_FALSE(selectResult.second.next());
    }

    void thenAllRecordsExistAndSplitCorreclty()
    {
        auto selectResult = m_relayPool->getConnection()->executeSelect(
            "SELECT * from cdb.relay_peers;").get();

        ASSERT_EQ(CASS_OK, selectResult.first);

        boost::optional<std::string> suffix1, suffix2, suffix3, domainTail, relayHost;
        boost::optional<cassandra::Uuid> relayId;

        int rowCount = 0;
        while (selectResult.second.next())
        {
            ++rowCount;
            ASSERT_TRUE(selectResult.second.get(0, &suffix1));
            ASSERT_TRUE(selectResult.second.get(1, &suffix2));
            ASSERT_TRUE(selectResult.second.get(2, &suffix3));
            ASSERT_TRUE(selectResult.second.get(3, &domainTail));
            ASSERT_TRUE(selectResult.second.get(4, &relayHost));
            ASSERT_TRUE(selectResult.second.get(5, &relayId));

            ASSERT_TRUE((bool)relayId);
            ASSERT_TRUE((bool)relayHost);
            ASSERT_TRUE((bool)suffix1);
            ASSERT_TRUE((bool)suffix2);
            ASSERT_TRUE((bool)suffix3);
            ASSERT_TRUE((bool)domainTail);

            ASSERT_FALSE(relayId->uuidString.empty());

            auto testDataIt = std::find_if(m_relayToDomainTestData.cbegin(),
                m_relayToDomainTestData.cend(),
                [&](const std::tuple<std::string, std::string, std::string>& tp)
                {
                    auto splitDomainResult = getDomainParts(std::get<2>(tp));
                    const auto& reversedParts = splitDomainResult.first;
                    const auto& domainTailString = splitDomainResult.second;

                    if (relayHost != std::get<1>(tp))
                        return false;

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

    bool verifyFindRelayResult(
        const std::string& domain,
        const std::vector<std::string>& candidates)
    {
        auto relayHost = m_relayPool->findRelayByDomain(domain).get();
        for (const auto& candidate: candidates)
        {
            if (relayHost == candidate)
                return true;
        }

        return false;
    }

    void thenRelaysShouldBeFoundCorrectly()
    {
        ASSERT_TRUE(verifyFindRelayResult("com", {"relayHost1", "relayHost2", "relayHost4"}));
        ASSERT_TRUE(verifyFindRelayResult("network.ru", {"relayHost3"}));
        ASSERT_TRUE(verifyFindRelayResult("someServer4.subdomain.nx.network.org", {"relayHost5"}));
        ASSERT_TRUE(verifyFindRelayResult("someServer2.nx.network.ru", {"relayHost3"}));
    }

    void thenNewRelayInfoShouldBeFound()
    {
        auto relayHost = m_relayPool->findRelayByDomain("someServer.nx.com").get();
        ASSERT_FALSE(relayHost.empty());
        ASSERT_EQ(relayHost, "new_host");
    }

private:
    std::unique_ptr<TestRelayPool> m_relayPool;
    std::vector<std::tuple<std::string, std::string, std::string>> m_relayToDomainTestData;
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

TEST_F(RemoteRelayPeerPool, removePeer)
{
    givenDbWithNotExistentCdbKeyspace();
    whenRelayPoolObjectHasBeenCreated();
    whenFivePeersHaveBeenAdded();
    whenFivePeersHaveBeenRemoved();

    thenRelayPeersTableShouldBeEmpty();
}

TEST_F(RemoteRelayPeerPool, getRelayByDomain)
{
    givenDbWithNotExistentCdbKeyspace();
    whenRelayPoolObjectHasBeenCreated();
    whenTableIsFilledWithTestData();

    thenRelaysShouldBeFoundCorrectly();
}

TEST_F(RemoteRelayPeerPool, relayIdAndHostReplacedIfAddedFromAnotherRelay)
{
    givenDbWithNotExistentCdbKeyspace();
    whenRelayPoolObjectHasBeenCreated();
    whenTableIsFilledWithTestData();
    whenOneRecordIsReplacedWithDifferentRelayHostAndId();

    thenNewRelayInfoShouldBeFound();
}

} // namespace test
} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
