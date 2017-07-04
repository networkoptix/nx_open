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

    virtual void TearDown() override
    {
        auto removeKeySpaceFuture = m_connection.executeUpdate("DROP KEYSPACE test_space;");
        assertCassError(CASS_OK, removeKeySpaceFuture.get());
    }

    void assertCassError(CassError expected, CassError desired)
    {
        ASSERT_EQ(expected, desired);
    }

    cf::future<CassError> whenConnectionEstablished()
    {
        return m_connection.init();
    }

    template<typename T>
    void bindValAndAssert(Query* query, const std::string& key, const T& t)
    {
        ASSERT_TRUE(query->bind(key, t));
    }

    cf::future<CassError> whenTestTableCreated()
    {
        return m_connection.executeUpdate(
            "CREATE KEYSPACE test_space WITH replication = { \
                'class': 'SimpleStrategy', 'replication_factor': '2' };")
            .then(
                [this](cf::future<CassError> executeCreateKeySpaceFuture)
                {
                    assertCassError(CASS_OK, executeCreateKeySpaceFuture.get());
                    return m_connection.executeUpdate(
                        "CREATE TABLE test_space.test_table ( \
                            string_val  text, \
                            bool_val    boolean, \
                            double_val  double, \
                            float_val   float, \
                            int_val     int, \
                            int64_val   bigint, \
                            PRIMARY KEY (string_val) \
                        );");
                });
    }

    cf::future<CassError> whenTestRowInserted()
    {
        return m_connection.prepareQuery(
            "INSERT INTO test_space.test_table ( \
                string_val, bool_val, double_val, float_val, int_val, int64_val ) \
                VALUES (?, ?, ?, ?, ?, ?);")
            .then(
                [this](cf::future<std::pair<CassError, Query>> prepareFuture)
                {
                    auto prepareResult = prepareFuture.get();
                    assertCassError(CASS_OK, prepareResult.first);

                    bindValAndAssert(&prepareResult.second, "string_val", std::string("some text"));
                    bindValAndAssert(&prepareResult.second, "bool_val", true);
                    bindValAndAssert(&prepareResult.second, "double_val", 42.02);
                    bindValAndAssert(&prepareResult.second, "float_val", (float)0.242);
                    bindValAndAssert(&prepareResult.second, "int_val", (int32_t)11);
                    bindValAndAssert(&prepareResult.second, "int64_val", (int64_t)31);

                    return m_connection.executeUpdate(std::move(prepareResult.second));
                });
    }

    cf::future<CassError> thenSelectedDataShouldBeEqualToInserted()
    {
        return cf::make_ready_future(CassError(CASS_OK));
    }

    nx::cassandra::AsyncConnection m_connection;
};

TEST_F(Connection, InsertSelect_Async)
{
    whenConnectionEstablished()
        .then(
            [this](cf::future<CassError> f)
            {
                assertCassError(CASS_OK, f.get());
                return whenTestTableCreated();
            })
        .then(
            [this](cf::future<CassError> f) mutable
            {
                assertCassError(CASS_OK, f.get());
                return whenTestRowInserted();
            })
        .then(
            [this](cf::future<CassError> f) mutable
            {
                assertCassError(CASS_OK, f.get());
                return whenTestRowInserted();
            })
        .then(
            [this](cf::future<CassError> f) mutable
            {
                assertCassError(CASS_OK, f.get());
                return thenSelectedDataShouldBeEqualToInserted();
            }).wait();
}

} // namespace test
} // namespace cassandra
} // namespace nx
