#include <gtest/gtest.h>
#include <nx/casssandra/async_cassandra_connection.h>
#include "options.h"

namespace nx {
namespace cassandra {
namespace test {

using namespace nx::utils;

class Connection: public ::testing::Test
{
protected:
    struct BasicStruct
    {
        bool boolVal = true;
        double doubleVal = 42.02;
        float floatVal= 0.242;
        int32_t intVal = 11;
        int64_t int64Val = 32;
    };

    friend bool operator == (const BasicStruct& lhs, const BasicStruct& rhs)
    {
        return lhs.boolVal == rhs.boolVal
            && lhs.doubleVal == rhs.doubleVal
            && lhs.floatVal == rhs.floatVal
            && lhs.intVal == rhs.intVal
            && lhs.int64Val == rhs.int64Val;
    }

    Connection(): m_connection(options()->host.c_str()) {}

    virtual void TearDown() override
    {
        auto removeKeySpaceFuture = m_connection.executeUpdate("DROP KEYSPACE test_space;");
        assertCassError(CASS_OK, removeKeySpaceFuture.get());
    }

    static void assertCassError(CassError expected, CassError desired)
    {
        ASSERT_EQ(expected, desired);
    }

    static void assertStringsEqual(const std::string& lhs, const std::string& rhs)
    {
        ASSERT_EQ(lhs, rhs);
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

    template<typename T>
    static void getValAndAssert(const QueryResult& queryResult, const std::string& key, T* t)
    {
        ASSERT_TRUE(queryResult.get(key, t));
    }

    template<typename T>
    static void getValAndAssert(const QueryResult& queryResult, int index, T* t)
    {
        ASSERT_TRUE(queryResult.get(index, t));
    }


    static void assertBasicStructsEqual(const BasicStruct& lhs, const BasicStruct& rhs)
    {
        ASSERT_EQ(lhs, rhs);
    }

    static void assertRowCount(int expected, int desired)
    {
        ASSERT_EQ(expected, desired);
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
                            PRIMARY KEY (string_val, bool_val, double_val, float_val, int_val) \
                        );");
                });
    }

    cf::future<CassError> whenTestRowInserted(const std::string& key)
    {
        return m_connection.prepareQuery(
            "INSERT INTO test_space.test_table ( \
                string_val, bool_val, double_val, float_val, int_val, int64_val ) \
                VALUES (?, ?, ?, ?, ?, ?);")
            .then(
                [this, key](cf::future<std::pair<CassError, Query>> prepareFuture)
                {
                    auto prepareResult = prepareFuture.get();
                    assertCassError(CASS_OK, prepareResult.first);

                    bindValAndAssert(&prepareResult.second, "string_val", key);
                    bindValAndAssert(&prepareResult.second, "bool_val", m_basicStruct.boolVal);
                    bindValAndAssert(&prepareResult.second, "double_val", m_basicStruct.doubleVal);
                    bindValAndAssert(&prepareResult.second, "float_val", m_basicStruct.floatVal);
                    bindValAndAssert(&prepareResult.second, "int_val", m_basicStruct.intVal);
                    bindValAndAssert(&prepareResult.second, "int64_val", m_basicStruct.int64Val);

                    return m_connection.executeUpdate(std::move(prepareResult.second));
                });
    }

    static void getRowValuesAndCheckIfValidByName(
        const QueryResult& queryResult,
        std::string* key,
        BasicStruct* basicStruct)
    {
        getValAndAssert(queryResult, "string_val", key);
        getValAndAssert(queryResult, "bool_val", &basicStruct->boolVal);
        getValAndAssert(queryResult, "double_val", &basicStruct->doubleVal);
        getValAndAssert(queryResult, "float_val", &basicStruct->floatVal);
        getValAndAssert(queryResult, "int_val", &basicStruct->intVal);
        getValAndAssert(queryResult, "int64_val", &basicStruct->int64Val);
    }

    static void getRowValuesAndCheckIfValidByIndex(
        const QueryResult& queryResult,
        std::string* key,
        BasicStruct* basicStruct)
    {
        getValAndAssert(queryResult, 0, key);
        getValAndAssert(queryResult, 1, &basicStruct->boolVal);
        getValAndAssert(queryResult, 2, &basicStruct->doubleVal);
        getValAndAssert(queryResult, 3, &basicStruct->floatVal);
        getValAndAssert(queryResult, 4, &basicStruct->intVal);
        getValAndAssert(queryResult, 5, &basicStruct->int64Val);
    }

    template<typename GetRowFunc>
    cf::future<CassError> selectTestData(GetRowFunc getRowFunc)
    {
        return m_connection.prepareQuery("SELECT * FROM test_space.test_table;")
            .then(
                [this](cf::future<std::pair<CassError, Query>> prepareFuture)
                {
                    auto prepareResult = prepareFuture.get();
                    assertCassError(CASS_OK, prepareResult.first);

                    return m_connection.executeSelect(std::move(prepareResult.second));
                })
            .then(
                [this, getRowFunc](cf::future<std::pair<CassError, QueryResult>> selectFuture)
                {
                    BasicStruct basicStruct;
                    int rowCount = 0;
                    std::string key;

                    auto selectResult = selectFuture.get();
                    assertCassError(CASS_OK, selectResult.first);

                    while (selectResult.second.next())
                    {
                        ++rowCount;
                        getRowFunc(selectResult.second, &key, &basicStruct);
                        assertBasicStructsEqual(m_basicStruct, basicStruct);

                        if (rowCount == 1)
                            assertStringsEqual(key, "key1");
                        if (rowCount == 2)
                            assertStringsEqual(key, "key2");
                    }

                    assertRowCount(2, rowCount);
                    return selectResult.first;
                });
    }

    cf::future<CassError> thenAllSelectedByNameDataShouldBeEqualToInserted()
    {
        return selectTestData(&getRowValuesAndCheckIfValidByName);
    }

    cf::future<CassError> thenAllSelectedByIndexDataShouldBeEqualToInserted()
    {
        return selectTestData(&getRowValuesAndCheckIfValidByIndex);
    }

    nx::cassandra::AsyncConnection m_connection;
    BasicStruct m_basicStruct;
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
                return whenTestRowInserted("key1");
            })
        .then(
            [this](cf::future<CassError> f) mutable
            {
                assertCassError(CASS_OK, f.get());
                return whenTestRowInserted("key2");
            })
        .then(
            [this](cf::future<CassError> f) mutable
            {
                assertCassError(CASS_OK, f.get());
                return thenAllSelectedByNameDataShouldBeEqualToInserted();
            })
        .then(
            [this](cf::future<CassError> f) mutable
            {
                assertCassError(CASS_OK, f.get());
                return thenAllSelectedByIndexDataShouldBeEqualToInserted();
            }).wait();

}

} // namespace test
} // namespace cassandra
} // namespace nx
