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
    void givenOneDb()
    {
        addDbs(1);
    }

    void givenTwoDbs()
    {
        addDbs(2);
    }

    void givenDbWithRandomKeyValuePair()
    {
        givenOneDb();
        givenRandomKeyValuePair();
        whenInsertKeyValuePair();
        thenOperationSucceeded();
    }

    void givenDbWithTwoKnownKeyValuePairs()
    {
        givenOneDb();
        givenKnownKeyValuePair();
        givenSecondKnownKeyValuePair();

        whenInsertKeyValuePair();
        thenOperationSucceeded();

        whenInsertSecondKeyValuePair();
        thenOperationSucceeded();
    }

    void givenTwoSynchronizedDbsWithRandomKeyValuePair()
    {
        givenTwoDbs();
        whenInsertKeyValuePair();

        thenOperationSucceeded();
        andValueIsInDb();
        andValueIsInSecondDb();
    }

    void givenEmptyDb()
    {
        givenOneDb();
    }

    void givenKeyValuePairWithEmptyKey()
    {
        randomPair()->value = randomString();
    }

    void givenRandomKeyValuePair(size_t randomPairIndex = 0)
    {
        randomPair(randomPairIndex)->key = randomString();
        randomPair(randomPairIndex)->value = randomString();
    }

    void givenKnownKeyValuePair()
    {
        randomPair()->key = "A key";
        randomPair()->value = "A value";
    }

    void givenSecondKnownKeyValuePair()
    {
        randomPair(1)->key = "B key";
        randomPair(1)->value = "B value";
    }

    void givenTwoKeyValuePairsWithSameKey()
    {
        std::string sameKey = "Same Key";

        randomPair()->key = sameKey;
        randomPair()->value = "A Value";

        randomPair(1)->key = sameKey;
        randomPair(1)->value = "A Different Value";
    }

    void givenAnotherRandomKeyValuePair()
    {
        givenRandomKeyValuePair();
    }

    void givenRecordInsertedEventWasAlreadyTriggered()
    {
        givenEmptyDb();
        givenRandomKeyValuePair();

        whenSubscribeToRecordInsertedEvent();
        whenInsertKeyValuePair();

        thenEventTriggered();
        andEventReceivedKeyMatchesInsertionKey();
        andEventReceivedValueMatchesInsertionValue();

        m_eventTriggered = false;
        m_eventReceivedKey.clear();
        m_eventReceivedValue.clear();
    }

    void givenRecordRemovedEventWasAlreadyTriggered()
    {
        givenDbWithRandomKeyValuePair();

        whenSubscribeToRecordRemovedEvent();
        whenRemoveKey();

        thenOperationSucceeded();
        thenEventTriggered();
        andEventReceivedKeyMatchesRemovalKey();

        m_eventTriggered = false;
        m_eventReceivedKey.clear();
    }

    void whenInsertKeyValuePair(size_t dbIndex = 0, size_t randomPairIndex = 0)
    {
        db(dbIndex)->dataManager().insertOrUpdate(
            randomPair(randomPairIndex)->key,
            randomPair(randomPairIndex)->value,
            [this](map::ResultCode result)
            {
                m_result = result;
                callbackFired();
            });

        waitForCallback();
    }

    void whenInsertSecondKeyValuePair()
    {
        whenInsertKeyValuePair(/*dbIndex*/ 0, /*randomPairIndex*/ 1);
    }

    void whenFetchValue(size_t dbIndex = 0, size_t randomPairIndex = 0)
    {
        db(dbIndex)->dataManager().get(
            randomPair(randomPairIndex)->key,
            [this](map::ResultCode result, std::string value)
            {
                m_result = result;
                m_fetchedValue = value;
                callbackFired();
            });

        waitForCallback();
    }

    void whenRemoveKey()
    {
        db()->dataManager().remove(randomPair()->key,
            [this](map::ResultCode result)
            {
                m_result = result;
                callbackFired();
            });

        waitForCallback();
    }

    void whenRemoveNonExistentKey()
    {
        randomPair()->key += "-NonExistentKey";
        andValueIsNotInDb();
        whenRemoveKey();
    }

    void whenSubscribeToRecordInsertedEvent()
    {
        db()->eventProvider().subscribeToRecordInserted(
            [this](nx::sql::QueryContext* /*queryContext*/, std::string key, std::string value)
            {
                m_eventTriggered = true;
                m_eventReceivedKey = key;
                m_eventReceivedValue = value;
            },
            &m_eventSubscriptionId);
    }

    void whenSubscribeToRecordRemovedEvent()
    {
        db()->eventProvider().subscribeToRecordRemoved(
            [this](nx::sql::QueryContext* /*queryContext*/, std::string key)
            {
                m_eventTriggered = true;
                m_eventReceivedKey = key;
            },
            &m_eventSubscriptionId);
    }

    void whenUnsubscribeFromRecordInsertedEvent()
    {
        db()->eventProvider().unsubscribeFromRecordInserted(m_eventSubscriptionId);
    }

    void whenUnsubscribeFromRecordRemovedEvent()
    {
        db()->eventProvider().unsubscribeFromRecordRemoved(m_eventSubscriptionId);
    }

    void whenRequestLowerBoundForFirstKey()
    {
        whenRequestLowerBoundForKey();
    }

    void whenRequestLowerBoundForSecondKey()
    {
        whenRequestLowerBoundForKey(/*randomPairIndex*/ 1);
    }

    void whenRequestUpperBoundForFirstKey()
    {
        requestUpperBoundForKey();
    }

    void whenRequestUpperBoundForSecondKey()
    {
        requestUpperBoundForKey(/*randomPairIndex*/ 1);
    }

    void thenOperationSucceeded()
    {
        ASSERT_EQ(map::ResultCode::ok, m_result);
        m_result = map::ResultCode::unknownError;
    }

    void thenOperationFailed(map::ResultCode result)
    {
        ASSERT_EQ(result, m_result);
        m_result = map::ResultCode::unknownError;
    }

    void thenEventTriggered()
    {
        ASSERT_TRUE(m_eventTriggered.load());
    }

    void thenEventIsNotTriggered()
    {
        ASSERT_FALSE(m_eventTriggered.load());
    }

    void andFetchedValueMatchesRandomValue(size_t randomPairIndex = 0)
    {
        ASSERT_EQ(randomPair(randomPairIndex)->value, m_fetchedValue);
    }

    void andFetchedValueIsEmpty()
    {
        ASSERT_TRUE(m_fetchedValue.empty());
    }

    void thenLowerBoundMatchesFirstKey()
    {
        thenLowerBoundMatchesKey();
    }

    void thenLowerBoundMatchesSecondKey()
    {
        thenLowerBoundMatchesKey(/*ranomdPairIndex*/ 1);
    }

    void thenUpperBoundMatchesKey(size_t randomPairIndex = 0)
    {
        ASSERT_EQ(m_upperBound, randomPair(randomPairIndex)->key);
    }

    void thenUpperBoundMatchesFirstKey()
    {
        thenUpperBoundMatchesKey();
    }

    void thenUpperBoundMatchesSecondKey()
    {
        thenUpperBoundMatchesKey(/*ranomnPairIndex*/ 1);
    }

    void andValueIsInDb(size_t dbIndex = 0)
    {
        m_fetchedValue.clear();
        whenFetchValue(dbIndex);
        thenOperationSucceeded();
        andFetchedValueMatchesRandomValue();
    }

    void andValueIsInSecondDb()
    {
        andValueIsInDb(/*dbIndex*/ 1);
    }

    void andValueIsNotInDb(size_t dbIndex = 0)
    {
        whenFetchValue(dbIndex);
        thenOperationFailed(map::ResultCode::notFound);
        andFetchedValueIsEmpty();
    }

    void andValueIsNotInSecondDb()
    {
        andValueIsNotInDb(/*dbIndex*/ 1);
    }

    void andSecondValueReplacedFirst()
    {
        std::string oldValue = m_fetchedValue;

        // Search for value associated with first random pair, not second.
        whenFetchValue(/*dbIndex*/ 0, /*randomPairIndex*/ 0);
        thenOperationSucceeded();

        ASSERT_NE(oldValue, m_fetchedValue);
    }

    void andEventReceivedKeyMatchesInsertionKey()
    {
        ASSERT_EQ(m_eventReceivedKey, randomPair()->key);
    }

    void andEventReceivedKeyMatchesRemovalKey()
    {
        ASSERT_EQ(m_eventReceivedKey, randomPair()->key);
    }

    void andEventReceivedValueMatchesInsertionValue()
    {
        ASSERT_EQ(m_eventReceivedValue, randomPair()->value);
    }

    void andEventReceivedKeyDoesNotMatchInsertionKey()
    {
        ASSERT_NE(m_eventReceivedKey, randomPair()->key);
    }

    void andEventReceivedKeyDoesNotMatchRemovalKey()
    {
        ASSERT_NE(m_eventReceivedKey, randomPair()->key);
    }

private:
    Settings defaultDbSettings()
    {
        Settings settings;
        settings.dataSyncEngineSettings.maxConcurrentConnectionsFromSystem = 7;
        return settings;
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

    map::Database* db(size_t index = 0)
    {
        return m_databases[index].db.get();
    }

    void addDbs(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            auto queryExecutor = std::make_unique<nx::sql::AsyncSqlQueryExecutor>(
                dbConnectionOptions());

            ASSERT_TRUE(queryExecutor->init());

            m_databases.emplace_back(std::move(queryExecutor), defaultDbSettings());
        }
    }

    KeyValuePair* randomPair(size_t index = 0)
    {
        NX_ASSERT(0 <= (int)index && index <= m_randomPairs.size());

        if (index == m_randomPairs.size())
            m_randomPairs.emplace_back(KeyValuePair());

        return &m_randomPairs[index];
    }


    void whenRequestLowerBoundForKey(size_t randomPairIndex = 0)
    {
        db()->dataManager().lowerBound(
            randomPair(randomPairIndex)->key,
            [this](map::ResultCode result, std::string key)
        {
            m_result = result;
            m_lowerBound = key;
            callbackFired();
        });

        waitForCallback();
    }

    void requestUpperBoundForKey(size_t randomPairIndex = 0)
    {
        db()->dataManager().upperBound(
            randomPair(randomPairIndex)->key,
            [this](map::ResultCode result, std::string key)
        {
            m_result = result;
            m_upperBound = key;
            callbackFired();
        });

        waitForCallback();
    }

    void thenLowerBoundMatchesKey(size_t randomPairIndex = 0)
    {
        ASSERT_EQ(m_lowerBound, randomPair(randomPairIndex)->key);
    }


private:
    struct DBContext
    {
        std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> queryExecutor;
        std::unique_ptr<map::Database> db;

        DBContext(
            std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> queryExecutor,
            const Settings& dbSettings)
            :
            queryExecutor(std::move(queryExecutor)),
            db(std::make_unique<map::Database>(dbSettings, this->queryExecutor.get()))
        {
        }
    };

    std::vector<DBContext> m_databases;

    std::vector<KeyValuePair> m_randomPairs;
    map::ResultCode m_result;
    std::string m_fetchedValue;

    std::mutex m_mutex;
    std::condition_variable m_wait;
    std::atomic_bool m_callbackFired = false;

    nx::utils::SubscriptionId m_eventSubscriptionId = nx::utils::kInvalidSubscriptionId;
    std::atomic_bool m_eventTriggered = false;
    std::string m_eventReceivedKey;
    std::string m_eventReceivedValue;

    std::string m_lowerBound;
    std::string m_upperBound;
};

//-------------------------------------------------------------------------------------------------

TEST_F(Database, fetches_key_value_pair)
{
    givenDbWithRandomKeyValuePair();
    whenFetchValue();

    thenOperationSucceeded();
    andFetchedValueMatchesRandomValue();
}

TEST_F(Database, inserts_key_value_pair)
{
    givenEmptyDb();
    givenRandomKeyValuePair();
    whenInsertKeyValuePair();

    thenOperationSucceeded();
    andValueIsInDb();
}

TEST_F(Database, removes_key_value_pair)
{
    givenDbWithRandomKeyValuePair();
    whenRemoveKey();

    thenOperationSucceeded();
    andValueIsNotInDb();
}

TEST_F(Database, updates_key_value_pair)
{
    givenEmptyDb();
    givenTwoKeyValuePairsWithSameKey();

    whenInsertKeyValuePair();
    thenOperationSucceeded();
    andValueIsInDb();

    whenInsertSecondKeyValuePair();
    thenOperationSucceeded();
    andSecondValueReplacedFirst();
}

TEST_F(Database, handles_removing_non_existent_key)
{
    givenDbWithRandomKeyValuePair();
    whenRemoveNonExistentKey();

    thenOperationSucceeded();
}

TEST_F(Database, fails_to_fetch_non_existent_value)
{
    givenEmptyDb();
    givenRandomKeyValuePair();
    whenFetchValue();

    thenOperationFailed(map::ResultCode::notFound);
    andFetchedValueIsEmpty();
}

TEST_F(Database, rejects_insert_with_empty_key)
{
    givenOneDb();
    givenKeyValuePairWithEmptyKey();
    whenInsertKeyValuePair();

    thenOperationFailed(map::ResultCode::logicError);
}

TEST_F(Database, rejects_fetch_with_empty_string)
{
    givenOneDb();
    givenKeyValuePairWithEmptyKey();
    whenFetchValue();

    thenOperationFailed(map::ResultCode::logicError);
}

TEST_F(Database, rejects_remove_with_empty_string)
{
    givenOneDb();
    givenKeyValuePairWithEmptyKey();
    whenRemoveKey();

    thenOperationFailed(map::ResultCode::logicError);
}

TEST_F(Database, triggers_record_inserted_event)
{
    givenEmptyDb();
    givenRandomKeyValuePair();
    whenSubscribeToRecordInsertedEvent();
    whenInsertKeyValuePair();

    thenOperationSucceeded();
    thenEventTriggered();

    andEventReceivedKeyMatchesInsertionKey();
    andEventReceivedValueMatchesInsertionValue();
}

TEST_F(Database, triggers_record_removed_event)
{
    givenDbWithRandomKeyValuePair();
    whenSubscribeToRecordRemovedEvent();
    whenRemoveKey();

    thenOperationSucceeded();
    thenEventTriggered();

    andEventReceivedKeyMatchesRemovalKey();
}

TEST_F(Database, does_not_trigger_record_inserted_event_after_unsubscribing)
{
    givenRecordInsertedEventWasAlreadyTriggered();

    whenUnsubscribeFromRecordInsertedEvent();
    whenInsertKeyValuePair();

    thenEventIsNotTriggered();
    andEventReceivedKeyDoesNotMatchInsertionKey();
}

TEST_F(Database, does_not_trigger_record_removed_event_after_unsubscribing)
{
    givenRecordRemovedEventWasAlreadyTriggered();

    whenUnsubscribeFromRecordRemovedEvent();
    whenRemoveKey();

    thenEventIsNotTriggered();
    andEventReceivedKeyDoesNotMatchRemovalKey();
}

TEST_F(Database, provides_lowerbound)
{
    givenDbWithTwoKnownKeyValuePairs();

    whenRequestLowerBoundForFirstKey();

    thenOperationSucceeded();
    thenLowerBoundMatchesFirstKey();


    whenRequestLowerBoundForSecondKey();

    thenOperationSucceeded();
    thenLowerBoundMatchesSecondKey();
}

TEST_F(Database, provides_upperbound)
{
    givenDbWithTwoKnownKeyValuePairs();

    whenRequestUpperBoundForFirstKey();

    thenOperationSucceeded();
    thenUpperBoundMatchesSecondKey();
}

TEST_F(Database, fails_to_provide_lowerbound)
{
    givenEmptyDb();

    whenRequestLowerBoundForFirstKey();

    thenOperationFailed(map::ResultCode::notFound);
}

TEST_F(Database, fails_to_provide_upperbound)
{
    givenDbWithTwoKnownKeyValuePairs();

    whenRequestUpperBoundForSecondKey();

    thenOperationFailed(map::ResultCode::notFound);
}

TEST_F(Database, DISABLED_multiple_databases_synchronize_insert_operation)
{
    givenTwoDbs();
    givenRandomKeyValuePair();

    whenInsertKeyValuePair();

    thenOperationSucceeded();
    andValueIsInDb();
    andValueIsInSecondDb();
}

TEST_F(Database, DISABLED_multiple_databases_synchronize_remove_operation)
{
    givenTwoSynchronizedDbsWithRandomKeyValuePair();

    whenRemoveKey();
    thenOperationSucceeded();

    andValueIsNotInDb();
    andValueIsNotInSecondDb();
}

} // namespace nx::clusterdb::map::test