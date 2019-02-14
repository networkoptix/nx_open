#include <gtest/gtest.h>
#include <nx/sql/test_support/test_with_db_helper.h>

#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/types.h>
#include <nx/clusterdb/map/database.h>
#include <nx/clusterdb/map/settings.h>
#include <nx/clusterdb/map/test_support/node_launcher.h>
#include <nx/clusterdb/engine/http/http_paths.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>

namespace nx::clusterdb::map::test {

namespace {

std::optional<std::string> calculateUpperBound(const std::string& str)
{
    if (!str.empty())
    {
        std::string upperBound = str;
        for (auto rit = upperBound.rbegin(); rit != upperBound.rend(); ++rit)
        {
            if (++(*rit) != 0)
                return upperBound;
        }
    }
    return std::nullopt;
}

std::string randomString()
{
    return nx::utils::generateRandomName((rand() % 64) + 10).constData();
}

} // namespace

class Database:
    public testing::Test
{
protected:
    void givenOneDb()
    {
        addNodes(1);
    }

    void givenEmptyDb()
    {
        givenOneDb();
    }

    void givenTwoNodes()
    {
        addNodes(2);
    }

    void givenDbWithRandomKeyValuePair()
    {
        givenOneDb();
        givenRandomKeyValuePair();
        whenInsertKeyValuePair();
        thenOperationSucceeded();
    }

    void givenDbWithKnownKeyValuePairs()
    {
        addNodes(1);
        generateKeyValuePairs(/*startIndex*/ 0, /*count*/ 3, /*prefix*/{}, /*startKey*/'B');
    }

    void givenDbWithPrefixedKeyValuePairs()
    {
        m_prefix = "Prefix";

        addNodes(1);
        generateKeyValuePairs(/*startIndex*/ 0, /*count*/ 3, m_prefix, /*startKey*/ 'B', false);
        generateKeyValuePairs(3, 3, {}, 'B', true);
    }

    void givenDbWith0xffPrefixedKeyValuePairs()
    {
        m_prefix.assign(3, 0xff);

        addNodes(1);
        generateKeyValuePairs(/*startIndex*/ 0, /*count*/ 3, m_prefix, /*startKey*/ 'B');
        generateKeyValuePairs(3, 3, {}, 'B');
    }

    void givenSynchronizedNodesWithRandomKeyValuePair()
    {
        givenTwoNodes();
        givenRandomKeyValuePair();
        whenInsertKeyValuePair();

        thenOperationSucceeded();
        andValueIsInDb();
        andValueIsInSecondDb();
    }

    void givenKeyValuePairWithEmptyKey()
    {
        keyValuePair()->value = 'A';
    }

    void givenRandomKeyValuePair(size_t keyValuePairIndex = 0)
    {
        keyValuePair(keyValuePairIndex)->key = randomString();
        keyValuePair(keyValuePairIndex)->value = randomString();
    }

    void givenKnownKeyValuePair()
    {
        keyValuePair()->key ='B';
        keyValuePair()->value = 'B';
    }

    void givenSecondKnownKeyValuePair()
    {
        keyValuePair(1)->key = 'C';
        keyValuePair(1)->value = 'C';
    }

    void givenUninsertedLowerKeyValuePair()
    {
        char lowerKey = m_givenLowestKey[0] - 1;
        m_uninsertedPairIndex = m_keyValuePairs.size();

        keyValuePair(m_uninsertedPairIndex)->key = lowerKey;
        keyValuePair(m_uninsertedPairIndex)->value = lowerKey;
    }

    void givenUninsertedHigherKeyValuePair()
    {
        char higherKey = m_givenHighestKey[0] + 1;
        m_uninsertedPairIndex = m_keyValuePairs.size();

        keyValuePair(m_uninsertedPairIndex)->key = higherKey;
        keyValuePair(m_uninsertedPairIndex)->value = higherKey;
    }

    void givenTwoKeyValuePairsWithSameKey()
    {
        std::string sameKey = "Same Key";

        keyValuePair()->key = sameKey;
        keyValuePair()->value = "A Value";

        keyValuePair(1)->key = sameKey;
        keyValuePair(1)->value = "A Different Value";
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

    void whenInsertKeyValuePair(size_t dbIndex = 0, size_t keyValuePairIndex = 0)
    {
        db(dbIndex)->dataManager().insertOrUpdate(
            keyValuePair(keyValuePairIndex)->key,
            keyValuePair(keyValuePairIndex)->value,
            [this](map::ResultCode result)
            {
                m_result = result;
                callbackFired();
            });

        waitForCallback();
    }

    void whenInsertSecondKeyValuePair()
    {
        whenInsertKeyValuePair(/*dbIndex*/ 0, /*keyValuePairIndex*/ 1);
    }

    void whenFetchValue(size_t dbIndex = 0, size_t keyValuePairIndex = 0)
    {
        db(dbIndex)->dataManager().get(
            keyValuePair(keyValuePairIndex)->key,
            [this](map::ResultCode result, std::string value)
            {
                m_result = result;
                m_fetchedValue = std::move(value);
                callbackFired();
            });

        waitForCallback();
    }

    void whenRemoveKey()
    {
        db()->dataManager().remove(keyValuePair()->key,
            [this](map::ResultCode result)
            {
                m_result = result;
                callbackFired();
            });

        waitForCallback();
    }

    void whenRemoveNonExistentKey()
    {
        keyValuePair()->key += "-NonExistentKey";
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

    void whenRequestLowerBoundForLowestKey()
    {
        whenRequestLowerBoundForKey(m_givenLowestKey);
    }

    void whenRequestLowerBoundForHighestKey()
    {
        whenRequestLowerBoundForKey(m_givenHighestKey);
    }

    void whenRequestLowerBoundForUninsertedKey()
    {
        whenRequestLowerBoundForKey(m_uninsertedPairIndex);
    }

    void whenRequestUpperBoundForLowestKey()
    {
        whenRequestUpperBoundForKey(m_givenLowestKey);
    }

    void whenRequestUpperBoundForHighestKey()
    {
        whenRequestUpperBoundForKey(m_givenHighestKey);
    }

    void whenRequestRangeWithLowerBound()
    {
        whenRequestRange(m_givenLowestKey, m_givenHighestKey);
    }

    void whenRequestRangeWithUpperBound()
    {
        whenRequestRange(m_givenLowestKey, std::string(1, m_givenHighestKey[0] - 1));
    }

    void whenRequestRangeWithoutUpperBound()
    {
        whenRequestRange(m_givenLowestKey, std::string(1, m_givenHighestKey[0] + 1));
    }

    void whenRequestRangeWithoutLowerBound()
    {
        std::string keyWithoutLowerBound(1, m_givenHighestKey[0] + 1);
        std::string keyWithoutLowerBound2(1, m_givenHighestKey[0] + 2);
        whenRequestRange(keyWithoutLowerBound, keyWithoutLowerBound2);
    }

    void whenRequestRangeWithLowerBoundAndUpperBoundSwitched()
    {
        whenRequestRange(m_givenHighestKey, m_givenLowestKey);
    }

    void whenRequestRangeWithEqualLowerBoundAndUpperBound()
    {
        whenRequestRange(m_givenLowestKey, m_givenLowestKey);
    }

    void whenRequestPrefixedRange()
    {
        whenRequestPrefixedRange(m_prefix);
    }

    void whenRequestPrefixedRangeWithEmptyPrefix()
    {
        whenRequestPrefixedRange(/*prefix*/ {});
    }

    void whenRequestPrefixedRangeWithNonExistentPrefix()
    {
        whenRequestPrefixedRange(std::string("NonExistent") + m_prefix);
    }

    void whenRequestPrefixedRangeWithNonExistentPrefixSuffix()
    {
        whenRequestPrefixedRange(m_prefix + "NonExistent");
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

    void andFetchedValueMatchesGivenValue(size_t keyValuePairIndex = 0)
    {
        ASSERT_EQ(keyValuePair(keyValuePairIndex)->value, m_fetchedValue);
    }

    void andFetchedRangeMatchesExpectedRange()
    {
        ASSERT_EQ(m_fetchedRange, m_expectedRange);
    }

    void andFetchedValueIsEmpty()
    {
        ASSERT_TRUE(m_fetchedValue.empty());
    }

    void thenFetchedLowerBoundMatchesLowestKey()
    {
        thenLowerBoundMatchesKey(m_givenLowestKey);
    }

    void thenFetchedLowerBoundMatchesHighestKey()
    {
        thenLowerBoundMatchesKey(m_givenHighestKey);
    }

    void thenFetchedUpperBoundMatchesExpectedKey()
    {
        thenFetchedUpperBoundMatchesKey(std::string(1, m_givenLowestKey[0] + 1));
    }

    void thenFetchedUpperBoundMatchesKey(size_t keyValuePairIndex = 0)
    {
        thenFetchedUpperBoundMatchesKey(keyValuePair(keyValuePairIndex)->key);
    }

    void thenFetchedUpperBoundMatchesKey(const std::string& key)
    {
        ASSERT_EQ(m_upperBound, key);
    }

    void andValueIsInDb(size_t dbIndex = 0, size_t keyValuePairIndex = 0)
    {
        m_fetchedValue.clear();
        whenFetchValue(dbIndex, keyValuePairIndex);
        thenOperationSucceeded();
        andFetchedValueMatchesGivenValue(keyValuePairIndex);
    }

    void andValueIsInSecondDb()
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
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
        std::this_thread::sleep_for(std::chrono::seconds(1));
        andValueIsNotInDb(/*dbIndex*/ 1);
    }

    void andSecondValueReplacedFirst()
    {
        std::string oldValue = m_fetchedValue;

        // Search for value associated with first random pair, not second.
        whenFetchValue(/*dbIndex*/ 0, /*keyValuePairIndex*/ 0);
        thenOperationSucceeded();

        ASSERT_NE(oldValue, m_fetchedValue);
    }

    void andEventReceivedKeyMatchesInsertionKey()
    {
        ASSERT_EQ(m_eventReceivedKey, keyValuePair()->key);
    }

    void andEventReceivedKeyMatchesRemovalKey()
    {
        ASSERT_EQ(m_eventReceivedKey, keyValuePair()->key);
    }

    void andEventReceivedValueMatchesInsertionValue()
    {
        ASSERT_EQ(m_eventReceivedValue, keyValuePair()->value);
    }

    void andEventReceivedKeyDoesNotMatchInsertionKey()
    {
        ASSERT_NE(m_eventReceivedKey, keyValuePair()->key);
    }

    void andEventReceivedKeyDoesNotMatchRemovalKey()
    {
        ASSERT_NE(m_eventReceivedKey, keyValuePair()->key);
    }

private:
    nx::utils::Url nodeUrl(Node* node)
    {
        using namespace nx::clusterdb::engine;
        return nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(node->httpEndpoints().front());
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
        return m_nodes[index]->moduleInstance()->database();
    }

    void addNodes(int count)
    {
        ASSERT_TRUE(count > 0);

        for (int i = 0; i < count; ++i)
        {
            m_nodes.emplace_back(std::make_unique<NodeLauncher>());
            ASSERT_TRUE(m_nodes.back()->startAndWaitUntilStarted());
        }

        for (int i = 0; i < count; ++i)
        {
            for (int j = 0; j < count; ++j)
            {
                // Don't want any nodes connecting to themselves
                if (i == j)
                    continue;

                Node* node = m_nodes[j]->moduleInstance().get();

                m_nodes[i]->moduleInstance()->connectToNode(
                    node->database()->systemId(),
                    nodeUrl(node));
            }
        }
    }

    KeyValuePair* keyValuePair(size_t index = 0)
    {
        NX_ASSERT(0 <= (int)index && index <= m_keyValuePairs.size());

        if (index == m_keyValuePairs.size())
            m_keyValuePairs.emplace_back(KeyValuePair());

        return &m_keyValuePairs[index];
    }

    void generateKeyValuePairs(
        size_t startIndex,
        size_t count,
        const std::string& prefix,
        char startKey,
        bool findEdgeKeys = true)
    {
        char letter = startKey;
        for (
            size_t keyValuePairIndex = startIndex;
            keyValuePairIndex < startIndex + count;
            ++keyValuePairIndex, ++letter)
        {
            keyValuePair(keyValuePairIndex)->key = prefix + letter;
            keyValuePair(keyValuePairIndex)->value = prefix + letter;

            whenInsertKeyValuePair(/*dbIndex*/ 0, keyValuePairIndex);

            thenOperationSucceeded();
            andValueIsInDb(/*dbIndex*/ 0, keyValuePairIndex);
        }

        if(findEdgeKeys)
            this->findEdgeKeys();
    }

    void findEdgeKeys()
    {
        ASSERT_FALSE(m_keyValuePairs.empty());

        const auto compare =
            [](const KeyValuePair& a, const KeyValuePair& b)
            {
                return a.key < b.key;
            };

        m_givenHighestKey =
            std::max_element(m_keyValuePairs.begin(), m_keyValuePairs.end(), compare)->key;

        m_givenLowestKey =
            std::min_element(m_keyValuePairs.begin(), m_keyValuePairs.end(), compare)->key;
    }

    void whenRequestLowerBoundForKey(size_t keyValuePairIndex = 0)
    {
        whenRequestLowerBoundForKey(keyValuePair(keyValuePairIndex)->key);
    }

    void whenRequestLowerBoundForKey(const std::string& lowerBoundKey)
    {
        db()->dataManager().lowerBound(
            lowerBoundKey,
            [this](map::ResultCode result, std::string key)
        {
            m_result = result;
            m_fetchedLowerBound = std::move(key);
            callbackFired();
        });

        waitForCallback();
    }

    void whenRequestUpperBoundForKey(size_t keyValuePairIndex = 0)
    {
        whenRequestUpperBoundForKey(keyValuePair(keyValuePairIndex)->key);
    }

    void whenRequestUpperBoundForKey(const std::string& upperBoundKey)
    {
        db()->dataManager().upperBound(
            upperBoundKey,
            [this](map::ResultCode result, std::string key)
            {
                m_result = result;
                m_upperBound = std::move(key);
                callbackFired();
            });

        waitForCallback();
    }

    void whenRequestRange(size_t lowerBoundIndex, size_t upperBoundIndex)
    {
        whenRequestRange(keyValuePair(lowerBoundIndex)->key, keyValuePair(upperBoundIndex)->key);
    }

    void whenRequestRange(const std::string& lowerBoundKey, const std::string& upperBoundKey)
    {
        if (lowerBoundKey <= upperBoundKey)
        {
            auto allPairs = keyValuePairsAsMap();
            m_expectedRange = std::map<std::string, std::string>(
                allPairs.lower_bound(lowerBoundKey),
                allPairs.upper_bound(upperBoundKey));
        }

        db()->dataManager().getRange(
            lowerBoundKey,
            upperBoundKey,
            [this](ResultCode result, std::map<std::string, std::string> keyValuePairs)
            {
                m_result = result;
                m_fetchedRange = std::move(keyValuePairs);
                callbackFired();
            });

        waitForCallback();
    }

    void whenRequestPrefixedRange(const std::string& prefix)
    {
        std::string prefixLowerBound = prefix;
        auto prefixUpperBound = calculateUpperBound(prefix);

        auto allPairs = keyValuePairsAsMap();
        m_expectedRange = std::map<std::string, std::string>(
            allPairs.lower_bound(prefixLowerBound),
            prefixUpperBound.has_value()
                ? allPairs.upper_bound(*prefixUpperBound)
                : allPairs.end());

        db()->dataManager().getRangeWithPrefix(
            prefix,
            [this](map::ResultCode result, std::map<std::string, std::string> fetchedRange)
            {
                m_result = result;
                m_fetchedRange = std::move(fetchedRange);
                callbackFired();
            });

        waitForCallback();
    }

    void thenLowerBoundMatchesKey(size_t keyValuePairIndex = 0)
    {
        thenLowerBoundMatchesKey(keyValuePair(keyValuePairIndex)->key);
    }

    void thenLowerBoundMatchesKey(const std::string& key)
    {
        ASSERT_EQ(m_fetchedLowerBound, key);
    }

    std::map<std::string, std::string> keyValuePairsAsMap()
    {
        std::map<std::string, std::string> pairs;
        for (const auto& pair : m_keyValuePairs)
            pairs.emplace(pair.key, pair.value);
        return pairs;
    }

private:
    std::vector<std::unique_ptr<NodeLauncher>> m_nodes;

    std::vector<KeyValuePair> m_keyValuePairs;
    map::ResultCode m_result;
    std::string m_fetchedValue;

    std::map<std::string, std::string> m_expectedRange;
    std::map<std::string, std::string> m_fetchedRange;

    std::mutex m_mutex;
    std::condition_variable m_wait;
    std::atomic_bool m_callbackFired = false;

    nx::utils::SubscriptionId m_eventSubscriptionId = nx::utils::kInvalidSubscriptionId;
    std::atomic_bool m_eventTriggered = false;
    std::string m_eventReceivedKey;
    std::string m_eventReceivedValue;

    std::string m_fetchedLowerBound;
    std::string m_upperBound;

    size_t m_uninsertedPairIndex;

    std::string m_givenLowestKey;
    std::string m_givenHighestKey;

    std::string m_prefix;
};

//-------------------------------------------------------------------------------------------------

TEST_F(Database, fetches_key_value_pair)
{
    givenDbWithRandomKeyValuePair();
    whenFetchValue();

    thenOperationSucceeded();
    andFetchedValueMatchesGivenValue();
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

TEST_F(Database, fetches_lowerbound_for_lowest_key)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestLowerBoundForLowestKey();

    thenOperationSucceeded();
    thenFetchedLowerBoundMatchesLowestKey();
}


TEST_F(Database, fetches_lowerbound_for_highest_key)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestLowerBoundForHighestKey();

    thenOperationSucceeded();
    thenFetchedLowerBoundMatchesHighestKey();
}

TEST_F(Database, fetches_upperbound)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestUpperBoundForLowestKey();

    thenOperationSucceeded();
    thenFetchedUpperBoundMatchesExpectedKey();
}

TEST_F(Database, fails_to_fetch_upperbound_when_key_has_none)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestUpperBoundForHighestKey();

    thenOperationFailed(map::ResultCode::notFound);
}

TEST_F(Database, rejects_lowerbound_request_with_empty_key)
{
    givenOneDb();
    givenKeyValuePairWithEmptyKey();

    whenRequestLowerBoundForLowestKey();

    thenOperationFailed(map::ResultCode::logicError);
}

TEST_F(Database, rejects_upperbound_request_with_empty_key)
{
    givenOneDb();
    givenKeyValuePairWithEmptyKey();

    whenRequestLowerBoundForLowestKey();

    thenOperationFailed(map::ResultCode::logicError);
}

TEST_F(Database, fetches_lowerbound_when_key_goes_before_all_db_keys)
{
    givenDbWithKnownKeyValuePairs();
    givenUninsertedLowerKeyValuePair();

    whenRequestLowerBoundForUninsertedKey();

    thenOperationSucceeded();
}

TEST_F(Database, fails_to_fetch_lowerbound_when_key_goes_after_all_db_keys)
{
    givenDbWithKnownKeyValuePairs();
    givenUninsertedHigherKeyValuePair();

    whenRequestLowerBoundForUninsertedKey();

    thenOperationFailed(map::ResultCode::notFound);
}

TEST_F(Database, fetches_range_with_upperbound)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestRangeWithUpperBound();

    thenOperationSucceeded();
    andFetchedRangeMatchesExpectedRange();
}

TEST_F(Database, fetches_range_without_upperbound)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestRangeWithoutUpperBound();

    thenOperationSucceeded();
    andFetchedRangeMatchesExpectedRange();
}

TEST_F(Database, fetches_range_with_lowerbound)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestRangeWithLowerBound();

    thenOperationSucceeded();
    andFetchedRangeMatchesExpectedRange();
}

TEST_F(Database, fails_to_fetch_range_when_key_has_no_lowerbound)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestRangeWithoutLowerBound();

    thenOperationFailed(map::ResultCode::notFound);
}

TEST_F(Database, rejects_fetch_range_when_lowerbound_is_higher_than_upperbound)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestRangeWithLowerBoundAndUpperBoundSwitched();

    thenOperationFailed(map::ResultCode::logicError);
}

TEST_F(Database, fetches_range_when_lowerbound_and_upperbound_are_equal)
{
    givenDbWithKnownKeyValuePairs();

    whenRequestRangeWithEqualLowerBoundAndUpperBound();

    thenOperationSucceeded();
    andFetchedRangeMatchesExpectedRange();
}

TEST_F(Database, fetches_range_with_prefix)
{
    givenDbWithPrefixedKeyValuePairs();

    whenRequestPrefixedRange();

    thenOperationSucceeded();

    andFetchedRangeMatchesExpectedRange();
}

TEST_F(Database, rejects_fetch_range_when_prefix_is_empty)
{
    givenDbWithPrefixedKeyValuePairs();

    whenRequestPrefixedRangeWithEmptyPrefix();

    thenOperationFailed(map::ResultCode::logicError);
}

TEST_F(Database, fails_to_fetch_range_when_prefix_is_not_found)
{
    givenDbWithPrefixedKeyValuePairs();

    whenRequestPrefixedRangeWithNonExistentPrefix();

    thenOperationFailed(map::ResultCode::notFound);
}


TEST_F(Database, fails_to_fetch_range_when_prefix_has_non_existent_suffix)
{
    givenDbWithPrefixedKeyValuePairs();

    whenRequestPrefixedRangeWithNonExistentPrefixSuffix();

    thenOperationFailed(map::ResultCode::notFound);
}

TEST_F(Database, fetches_range_with_0xff_prefix)
{
    givenDbWith0xffPrefixedKeyValuePairs();

    whenRequestPrefixedRange();

    thenOperationSucceeded();

    andFetchedRangeMatchesExpectedRange();
}

TEST_F(Database, multiple_nodes_synchronize_insert_operation)
{
    givenTwoNodes();
    givenRandomKeyValuePair();

    whenInsertKeyValuePair();

    thenOperationSucceeded();
    andValueIsInDb();
    andValueIsInSecondDb();
}

TEST_F(Database, multiple_nodes_synchronize_remove_operation)
{
    givenSynchronizedNodesWithRandomKeyValuePair();

    whenRemoveKey();

    thenOperationSucceeded();
    andValueIsNotInDb();
    andValueIsNotInSecondDb();
}

} // namespace nx::clusterdb::map::test