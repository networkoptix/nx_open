#include <gtest/gtest.h>
#include <nx/casssandra/async_cassandra_connection.h>

namespace nx {
namespace cassandra {
namespace test {

using namespace nx::utils;

class Connection: public ::testing::Test
{
protected:
    Connection(): m_connection("127.0.0.1") {}

    void assertCassError(CassError expected, CassError desired)
    {
        ASSERT_EQ(expected, desired);
    }

    void whenSomeTestDataInserted()
    {
        m_connection.init()
            .then(
                [this](cf::future<CassError> connectFuture) mutable
                {
                    assertCassError(CASS_OK, connectFuture.get());
                    return m_connection.prepareQuery(
                        "CREATE KEYSPACE test_space WITH replication = { \
                            'class': 'SimpleStrategy', 'replication_factor': '2' };");
                })
            .then(
                [this](cf::future<std::pair<CassError, Query>> prepareKeySpaceFuture) mutable
                {
                    auto held = prepareKeySpaceFuture.get();
                    assertCassError(CASS_OK, held.first);
                    return m_connection.executeUpdate(std::move(held.second));
                })
            .then(
                [this](cf::future<CassError> executeCreateKeySpaceFuture)
                {
                    assertCassError(CASS_OK, executeCreateKeySpaceFuture.get());
                    return cf::unit();
                }).wait();
    }

    nx::cassandra::AsyncConnection m_connection;
};

TEST_F(Connection, InsertSelect_Async)
{
    whenSomeTestDataInserted();
}

} // namespace test
} // namespace cassandra
} // namespace nx
