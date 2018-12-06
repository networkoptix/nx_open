#include <string.h>
#include <gtest/gtest.h>
#include <nx/casssandra/async_cassandra_connection.h>
#include "options.h"

namespace nx {
namespace cassandra {
namespace test {

using namespace nx::utils;

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#   define stricmp strcasecmp
#endif

class Connection: public ::testing::Test
{
protected:
    struct BasicStruct
    {
        bool boolVal = true;
        double doubleVal = 42.02;
        float floatVal= (float)0.242;
        int32_t intVal = 11;
        int64_t int64Val = 32;
        InetAddr inetVal{"17.3.90.11"};
        Uuid uuidVal{"DC93190E-A24F-4057-93E1-E31EEA0129A6"};
    };

    friend bool operator == (const BasicStruct& lhs, const BasicStruct& rhs)
    {
        return lhs.boolVal == rhs.boolVal
            && lhs.doubleVal == rhs.doubleVal
            && lhs.floatVal == rhs.floatVal
            && lhs.intVal == rhs.intVal
            && lhs.int64Val == rhs.int64Val
            && strcmp(lhs.inetVal.addrString.c_str(), rhs.inetVal.addrString.c_str()) == 0
            && stricmp(lhs.uuidVal.uuidString.c_str(), rhs.uuidVal.uuidString.c_str()) == 0;
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

    void bindNullValAndAssert(Query* query, const std::string& key)
    {
        ASSERT_TRUE(query->bindNull(key));
    }

    template<typename T>
    static void getValAndAssert(const QueryResult& queryResult, const std::string& key, T* t)
    {
        boost::optional<T> resultValue;
        ASSERT_TRUE(queryResult.get(key, &resultValue));
        ASSERT_TRUE((bool)resultValue);
        *t = *resultValue;
    }

    template<typename T>
    static void getNullValAndAssert(const QueryResult& queryResult, const std::string& key)
    {
        boost::optional<T> resultValue;
        auto getResult = queryResult.get(key, &resultValue);
        ASSERT_TRUE(getResult);
        ASSERT_FALSE((bool)resultValue);
    }

    template<typename T>
    static void getValAndAssert(const QueryResult& queryResult, int index, T* t)
    {
        boost::optional<T> resultValue;
        ASSERT_TRUE(queryResult.get(index, &resultValue));
        ASSERT_TRUE((bool)resultValue);
        *t = *resultValue;
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
                            inet_val    inet, \
                            uuid_val    uuid, \
                            PRIMARY KEY (string_val, bool_val, double_val, \
                                float_val, int_val, int64_val, inet_val, uuid_val ) \
                        );");
                });
    }

    cf::future<CassError> whenTestTableWithNullableValuesCreated()
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
                            key         text, \
                            string_val  text, \
                            bool_val    boolean, \
                            double_val  double, \
                            float_val   float, \
                            int_val     int, \
                            int64_val   bigint, \
                            inet_val    inet, \
                            uuid_val    uuid, \
                            PRIMARY KEY (key) \
                        );");
                });
    }

    cf::future<CassError> whenTestRowInserted(const std::string& key)
    {
        return m_connection.prepareQuery(
            "INSERT INTO test_space.test_table ( \
                string_val, bool_val, double_val, float_val, \
                int_val, int64_val, inet_val, uuid_val ) \
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?);")
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
                    bindValAndAssert(&prepareResult.second, "inet_val", m_basicStruct.inetVal);
                    bindValAndAssert(&prepareResult.second, "uuid_val", m_basicStruct.uuidVal);

                    return m_connection.executeUpdate(std::move(prepareResult.second));
                });
    }

    cf::future<CassError> whenTestRowWithNullValuesInserted(const std::string& key)
    {
        return m_connection.prepareQuery(
            "INSERT INTO test_space.test_table ( \
                key, string_val, bool_val, double_val, float_val, \
                int_val, int64_val, inet_val, uuid_val ) \
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);")
            .then(
                [this, key](cf::future<std::pair<CassError, Query>> prepareFuture)
                {
                    auto prepareResult = prepareFuture.get();
                    assertCassError(CASS_OK, prepareResult.first);

                    bindValAndAssert(&prepareResult.second, "key", key);
                    bindNullValAndAssert(&prepareResult.second, "string_val");
                    bindNullValAndAssert(&prepareResult.second, "int_val");

                    // Other fields must be set to NULL implicitly

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
        getValAndAssert(queryResult, "inet_val", &basicStruct->inetVal);
        getValAndAssert(queryResult, "uuid_val", &basicStruct->uuidVal);
    }

    static void getRowValuesWithNullsAndCheckIfValidByName(
        const QueryResult& queryResult,
        std::string* key,
        BasicStruct* /*basicStruct*/)
    {
        getValAndAssert(queryResult, "key", key);
        getNullValAndAssert<std::string>(queryResult, "string_val");
        getNullValAndAssert<bool>(queryResult, "bool_val");
        getNullValAndAssert<double>(queryResult, "double_val");
        getNullValAndAssert<float>(queryResult, "float_val");
        getNullValAndAssert<int>(queryResult, "int_val");
        getNullValAndAssert<int64_t>(queryResult, "int64_val");
        getNullValAndAssert<InetAddr>(queryResult, "inet_val");
        getNullValAndAssert<Uuid>(queryResult, "uuid_val");
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
        getValAndAssert(queryResult, 6, &basicStruct->inetVal);
        getValAndAssert(queryResult, 7, &basicStruct->uuidVal);
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

    cf::future<CassError> thenAllSelectedByNameDataWithNullsShouldBeEqualToInserted()
    {
        return selectTestData(&getRowValuesWithNullsAndCheckIfValidByName);
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

TEST_F(Connection, HandlingNullValues)
{
    whenConnectionEstablished()
        .then(
            [this](cf::future<CassError> f)
            {
                assertCassError(CASS_OK, f.get());
                return whenTestTableWithNullableValuesCreated();
            })
        .then(
            [this](cf::future<CassError> f) mutable
            {
                assertCassError(CASS_OK, f.get());
                return whenTestRowWithNullValuesInserted("key1");
            })
        .then(
            [this](cf::future<CassError> f) mutable
            {
                assertCassError(CASS_OK, f.get());
                return whenTestRowWithNullValuesInserted("key2");
            })
        .then(
            [this](cf::future<CassError> f) mutable
            {
                assertCassError(CASS_OK, f.get());
                return thenAllSelectedByNameDataWithNullsShouldBeEqualToInserted();
            }).wait();
        //.then(
        //    [this](cf::future<CassError> f) mutable
        //    {
        //        assertCassError(CASS_OK, f.get());
        //        return thenAllSelectedByIndexDataWithNullsShouldBeEqualToInserted();
        //    }).wait();
}

} // namespace test
} // namespace cassandra
} // namespace nx
