#include <gtest/gtest.h>
#include <nx/sql/test_support/test_with_db_helper.h>

#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/types.h>
#include <nx/clusterdb/map/database.h>
#include <nx/clusterdb/map/settings.h>

namespace nx::clusterdb::map::test {

namespace {

std::string randomString()
{
    return nx::utils::generateRandomName((rand() % 64) + 10).constData();
}

} // namespace

class Database:
    public testing::Test,
    public nx::sql::test::TestWithDbHelper
{
public:
    Database():
        TestWithDbHelper("nx_clusterdb_map", QString())
    {
    }

protected:
    void SetUp() override
    {
        ASSERT_TRUE(initializeQueryExecutor());

        m_db = std::make_unique<map::Database>(
            defaultDbSettings(),
            m_queryExecutor.get());
    }

    void whenSaveKeyValuePair()
    {
        m_db->dataManager().save(m_randomPair.key, m_randomPair.value,
            [this](map::ResultCode result)
            {
                m_result = result;
                callbackFired();
            });

        waitForCallback();
    }

    void whenSaveSameKeyValuePairTwice()
    {
        whenSaveKeyValuePair();
        thenOperationSucceeded();
        whenSaveKeyValuePair();
    }

    void thenOperationSucceeded()
    {
        ASSERT_EQ(map::ResultCode::ok, m_result);
    }

    void thenOperationFailed(map::ResultCode result)
    {
        ASSERT_EQ(result, m_result);
    }

    void givenDatabaseWithRandomKeyValuePair()
    {
        givenRandomKeyValuePair();
        whenSaveKeyValuePair();
        thenOperationSucceeded();
    }

    void givenEmptyDatabase()
    {
        givenRandomKeyValuePair();
        // Don't insert random pair.
    }

    void givenKeyValuePairWithEmptyKey()
    {
        m_randomPair.value = randomString();
    }

    void givenRandomKeyValuePair()
    {
        m_randomPair.key = randomString();
        m_randomPair.value = randomString();
    }

    void givenAnotherRandomKeyValuePair()
    {
        givenRandomKeyValuePair();
    }

    void whenFetchValue()
    {
        m_db->dataManager().get(m_randomPair.key,
            [this](map::ResultCode result, std::string value)
            {
                m_result = result;
                m_fetchedValue = value;
                callbackFired();
            });

        waitForCallback();
    }

    void andFetchedValueMatchesRandomValue()
    {
        ASSERT_EQ(m_randomPair.value, m_fetchedValue);
    }

    void whenRemoveKey()
    {
        m_db->dataManager().remove(m_randomPair.key,
            [this](map::ResultCode result)
            {
                m_result = result;
                callbackFired();
            });

        waitForCallback();
    }

    void whenRemoveNonExistentKey()
    {
        m_randomPair.key = "mangledKey";
        whenRemoveKey();
    }

    void andValueIsNotInDatabase()
    {
        whenFetchValue();
        thenOperationFailed(map::ResultCode::notFound);
        andFetchedValueIsEmpty();
    }

    void andFetchedValueIsEmpty()
    {
        ASSERT_TRUE(m_fetchedValue.empty());
    }

private:
    Settings defaultDbSettings()
    {
        Settings settings;
        settings.dataSyncEngineSettings.maxConcurrentConnectionsFromSystem = 7;
        return settings;
    }

    bool initializeQueryExecutor()
    {
        m_queryExecutor = std::make_unique<nx::sql::AsyncSqlQueryExecutor>(
            dbConnectionOptions());
        return m_queryExecutor->init();
    }

    void waitForCallback()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wait.wait(lock, [this]() { return m_callbackFired.load(); });
        m_callbackFired = false;
    }

    void callbackFired()
    {
        m_callbackFired = true;
        m_wait.notify_all();
    }

private:
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_queryExecutor;
    std::unique_ptr<map::Database> m_db;

    KeyValuePair m_randomPair;
    map::ResultCode m_result;
    std::string m_fetchedValue;

    std::mutex m_mutex;
    std::condition_variable m_wait;
    std::atomic_bool m_callbackFired = false;
};

//-------------------------------------------------------------------------------------------------

TEST_F(Database, saves_key_value_pair)
{
    givenRandomKeyValuePair();
    whenSaveKeyValuePair();

    thenOperationSucceeded();
}

TEST_F(Database, fetches_key_value_pair)
{
    givenDatabaseWithRandomKeyValuePair();
    whenFetchValue();

    thenOperationSucceeded();
    andFetchedValueMatchesRandomValue();
}

TEST_F(Database, removes_key_value_pair)
{
    givenDatabaseWithRandomKeyValuePair();
    whenRemoveKey();

    thenOperationSucceeded();
    andValueIsNotInDatabase();
}

TEST_F(Database, fails_to_fetch_non_existent_value)
{
    givenEmptyDatabase();
    whenFetchValue();

    thenOperationFailed(map::ResultCode::notFound);
    andFetchedValueIsEmpty();
}

TEST_F(Database, nothing_happens_when_removing_non_existent_key)
{
    givenDatabaseWithRandomKeyValuePair();
    whenRemoveNonExistentKey();

    thenOperationSucceeded();
}

TEST_F(Database, nothing_happens_when_saving_duplicate_key)
{
    givenRandomKeyValuePair();
    whenSaveSameKeyValuePairTwice();

    thenOperationSucceeded();
}

TEST_F(Database, rejects_save_with_empty_key)
{
    givenKeyValuePairWithEmptyKey();
    whenSaveKeyValuePair();

    thenOperationFailed(map::ResultCode::logicError);
}

TEST_F(Database, rejects_fetch_with_empty_string)
{
    givenKeyValuePairWithEmptyKey();
    whenFetchValue();

    thenOperationFailed(map::ResultCode::logicError);
}

TEST_F(Database, rejects_remove_with_empty_string)
{
    givenKeyValuePairWithEmptyKey();
    whenRemoveKey();

    thenOperationFailed(map::ResultCode::logicError);
}

} // namespace nx::clusterdb::map::test