// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/data_structures/time_out_cache.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

namespace nx::utils::test {

class TimeOutCache:
    public ::testing::Test
{
public:
    static constexpr auto kExpirationPeriod = std::chrono::hours(1) * 24 * 365;

    TimeOutCache(bool updateElementTimestampOnAccess = true):
        m_cache(kExpirationPeriod, 100, updateElementTimestampOnAccess),
        m_timeShift(test::ClockType::steady)
    {
    }

protected:
    void fillCache()
    {
        m_originalCacheItems.emplace_back(1, 111);
        m_originalCacheItems.emplace_back(2, 222);
        m_originalCacheItems.emplace_back(3, 333);

        for (const auto& [key, val]: m_originalCacheItems)
            m_cache.put(key, val);
    }

    void assertAddedItemIsFound()
    {
        ASSERT_EQ(
            m_originalCacheItems.front().second,
            m_cache.getValue(m_originalCacheItems.front().first));
    }

    void assertUnknownItemIsNotFound()
    {
        ASSERT_FALSE(m_cache.getValue(123));
    }

    void whenExpirationPeriodHasPassed()
    {
        m_timeShift.applyRelativeShift(kExpirationPeriod + std::chrono::milliseconds(1));
    }

    void whenTimeHasPassed(std::chrono::milliseconds period)
    {
        m_timeShift.applyRelativeShift(period);
    }

    void whenIterateThroughCache()
    {
        int count = 0;
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
        {
            // Just giving the loop something to do.
            ++count;
        }

        NX_DEBUG(this, "count is: %1", count);
    }

    void thenNoItemCanBeFound()
    {
        for (const auto& [key, val]: m_originalCacheItems)
        {
            ASSERT_FALSE(m_cache.contains(key));
            ASSERT_FALSE(m_cache.getValue(key));
        }
    }

private:
    std::vector<std::pair<int, int>> m_originalCacheItems;
    utils::TimeOutCache<int, int> m_cache;
    nx::utils::test::ScopedTimeShift m_timeShift;
};

TEST_F(TimeOutCache, non_expired_item_is_present)
{
    fillCache();

    assertAddedItemIsFound();
    assertUnknownItemIsNotFound();
}

TEST_F(TimeOutCache, expired_item_is_not_reported)
{
    fillCache();

    whenExpirationPeriodHasPassed();
    thenNoItemCanBeFound();
}

TEST_F(TimeOutCache, element_access_prolongs_its_life)
{
    fillCache();

    whenTimeHasPassed(kExpirationPeriod * 2 / 3);
    assertAddedItemIsFound();

    whenTimeHasPassed(kExpirationPeriod * 2 / 3);
    assertAddedItemIsFound();
}

TEST_F(TimeOutCache, begin_end_iteration_does_not_affect_item_expiration)
{
    fillCache();
    whenTimeHasPassed(kExpirationPeriod * 2);

    whenIterateThroughCache();

    thenNoItemCanBeFound();
}

//-------------------------------------------------------------------------------------------------

class TimeOutCacheWithoutProlongation:
    public TimeOutCache
{
public:
    TimeOutCacheWithoutProlongation():
        TimeOutCache(/*updateElementTimestampOnAccess*/ false)
    {
    }
};

TEST_F(TimeOutCacheWithoutProlongation, element_is_expired_regardless_of_the_usage)
{
    fillCache();
    assertAddedItemIsFound();

    whenTimeHasPassed(kExpirationPeriod * 2 / 3);
    // Fetching value from cache. The value expiration time must not be affected.
    assertAddedItemIsFound();

    whenTimeHasPassed(kExpirationPeriod * 2 / 3);
    thenNoItemCanBeFound();
}

} // namespace nx::utils::test
