// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/data_structures/lru_cache.h>

using IntIntVec = std::vector<std::pair<int, int>>;

namespace nx::utils::test {

class LruCache:
    public ::testing::Test
{
public:
    const size_t m_cacheSize = 4;
    nx::utils::LruCache<int, int> m_cache = nx::utils::LruCache<int, int>(m_cacheSize);

public:
    void whenPushElement(const std::pair<int, int>& el)
    {
        m_cache.put(el.first, el.second);
    }

    void thenElementsFromArrayInCache(
        const IntIntVec& testValues,
        int lBound,
        int rBound,
        std::set<int> excludedIndexes = std::set<int>())
    {
        for (int i = std::max(lBound, 0); i <= rBound; ++i)
        {
            if (excludedIndexes.count(i))
                continue;
            auto curVal = *m_cache.getValue(testValues[i].first);
            auto expectedVal = testValues[i].second;
            ASSERT_EQ(curVal, expectedVal);
        }
    }

    IntIntVec givenTestArray()
    {
        return IntIntVec{
            {1, 1}, {2, -1}, {3, 0}, {4, 11}, {5, 55}, {9, 2}};
    }
};

TEST_F(LruCache, elements_add)
{
    auto testValues = givenTestArray();
    for (size_t i = 0; i < testValues.size(); ++i)
    {
        whenPushElement(testValues[i]);
        thenElementsFromArrayInCache(testValues, i - m_cacheSize + 1, i);
    }
}

TEST_F(LruCache, element_lifetime_prolongated_at_lookup)
{
    auto testValues = givenTestArray();
    for (size_t i = 0; i < testValues.size(); ++i)
    {
        whenPushElement(testValues[i]);
        auto zeroVal = *m_cache.getValue(testValues[0].first);
        auto zeroExpectedVal = testValues[0].second;
        ASSERT_EQ(zeroVal, zeroExpectedVal);
    }
}

TEST_F(LruCache, element_update)
{
    IntIntVec testValues{{1, 1}, {2, 2}, {3, 3}, {1, -1}, {4, 4}, {5, 5}, {1, -2}, {1, -3}};
    for (size_t i = 0; i < 4; ++i)
        whenPushElement(testValues[i]);
    // {1, 1} was replaced
    thenElementsFromArrayInCache(testValues, 1, 3);
    for (size_t i = 4; i < 7; ++i)
        whenPushElement(testValues[i]);
    // {3, 3} still in cache
    thenElementsFromArrayInCache(testValues, 2, 6, std::set<int>{3});
    whenPushElement(testValues[7]);
    // {3, 3} still in cache
    thenElementsFromArrayInCache(testValues, 2, 7, std::set<int>{3, 6});
}

TEST_F(LruCache, element_erase)
{
    auto testValues = givenTestArray();
    for (size_t i = 0; i < m_cacheSize; ++i)
        whenPushElement(testValues[i]);
    for (size_t i = 0; i < m_cacheSize; ++i)
    {
        m_cache.erase(testValues[i].first);
        thenElementsFromArrayInCache(testValues, i + 1, m_cacheSize - 1);
        ASSERT_FALSE(m_cache.contains(testValues[i].first));
    }
}

TEST_F(LruCache, element_erase_by_own_ref)
{
    m_cache.put(1, 1);
    m_cache.erase(*m_cache.getValue(1));
    ASSERT_FALSE(m_cache.contains(1));
}

TEST_F(LruCache, forwarding_key)
{
    utils::LruCache<std::string, std::string> cache(100);
    cache.put(std::string("Hello"), std::string("world"));
    ASSERT_TRUE(cache.contains("Hello"));
}

} // namespace nx::utils::test
