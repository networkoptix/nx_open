// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <nx/vms/client/core/timeline/backends/data_lists_cache.h>

#include "test_backends.h"

namespace nx::vms::client::core::timeline {
namespace test {

using namespace std::chrono;

class TimelineDataListsCacheTest: public ::testing::Test
{
    using base_type = ::testing::Test;

public:
    using TimestampList = std::vector<milliseconds>;
    std::unique_ptr<DataListsCache<TimestampList>> cache;

    virtual void SetUp() override
    {
        base_type::SetUp();
        cache = std::make_unique<DataListsCache<TimestampList>>();
    }

    virtual void TearDown() override
    {
        cache.reset();
        base_type::TearDown();
    }

    const auto& firstBlock() const { return cache->cache().front(); }
    const auto& lastBlock() const { return cache->cache().back(); }
};

#define CHECK_INTEGRITY { \
    int count = 0; \
    EXPECT_EQ(cache->cache().size(), cache->lookup().size()); \
    for (auto blockIt = cache->cache().begin(); blockIt != cache->cache().end(); ++blockIt) \
    { \
        count += blockIt->size(); \
        const auto it = cache->lookup().find(blockIt->period().endTime()); \
        ASSERT_TRUE(it != cache->lookup().end()); \
        EXPECT_TRUE(it->second == blockIt); \
        if (const auto nextIt = std::next(it); nextIt != cache->lookup().end()) \
            EXPECT_GE(blockIt->period().startTime(), nextIt->first); \
    } \
    EXPECT_EQ(count, cache->cacheSize()); }

#define CHECK_BLOCK_DATA(block, expectedPeriod, expectedData) \
    EXPECT_EQ((block).period(), (expectedPeriod)); \
    EXPECT_EQ((block).data(), (expectedData));

#define CHECK_BLOCK(block, expectedPeriod, expectedData) \
    CHECK_BLOCK_DATA(block, expectedPeriod, expectedData); \
    EXPECT_TRUE((block).complete());

#define CHECK_INCOMPLETE_BLOCK(block, expectedTimestamp, expectedData) \
    CHECK_BLOCK_DATA(block, QnTimePeriod(expectedTimestamp, 1ms), expectedData); \
    EXPECT_TRUE((block).incomplete());

#define ASSERT_BLOCK_COUNT(expectedCount) \
    ASSERT_EQ(cache->cache().size(), expectedCount);

#define EXPECT_CACHE_SIZE(expectedSize) \
    EXPECT_EQ(cache->cacheSize(), expectedSize);

// ------------------------------------------------------------------------------------------------
// Complete blocks behavior.

TEST_F(TimelineDataListsCacheTest, addComplete)
{
    cache->add(interval(2ms, 1ms), kUnlimited, {1ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_EQ(cache->cacheSize(), 1);
    CHECK_BLOCK(firstBlock(), interval(2ms, 1ms), TimestampList({1ms}));
}

TEST_F(TimelineDataListsCacheTest, addSeparateLeft)
{
    cache->add(interval(2ms, 1ms), kUnlimited, {1ms});
    cache->add(interval(9ms, 5ms), kUnlimited, {7ms, 6ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(2ms, 1ms), TimestampList({1ms}));
    CHECK_BLOCK(lastBlock(), interval(9ms, 5ms), TimestampList({7ms, 6ms}));
}

TEST_F(TimelineDataListsCacheTest, addSeparateRight)
{
    cache->add(interval(9ms, 5ms), kUnlimited, {7ms, 6ms});
    cache->add(interval(2ms, 1ms), kUnlimited, {1ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(9ms, 5ms), TimestampList({7ms, 6ms}));
    CHECK_BLOCK(lastBlock(), interval(2ms, 1ms), TimestampList({1ms}));
}

TEST_F(TimelineDataListsCacheTest, addTouchingLeft)
{
    cache->add(interval(2ms, 1ms), kUnlimited, {1ms});
    cache->add(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(2ms, 1ms), TimestampList({1ms}));
    CHECK_BLOCK(lastBlock(), interval(5ms, 3ms), TimestampList({5ms, 4ms}));
}

TEST_F(TimelineDataListsCacheTest, addTouchingRight)
{
    cache->add(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    cache->add(interval(2ms, 1ms), kUnlimited, {1ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(5ms, 3ms), TimestampList({5ms, 4ms}));
    CHECK_BLOCK(lastBlock(), interval(2ms, 1ms), TimestampList({1ms}));
}

TEST_F(TimelineDataListsCacheTest, addOverlappingLeft)
{
    cache->add(interval(4ms, 1ms), kUnlimited, {4ms, 1ms});
    cache->add(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(2ms, 1ms), TimestampList({1ms}));
    CHECK_BLOCK(lastBlock(), interval(5ms, 3ms), TimestampList({5ms, 4ms}));
}

TEST_F(TimelineDataListsCacheTest, addOverlappingRight)
{
    cache->add(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    cache->add(interval(4ms, 1ms), kUnlimited, {4ms, 1ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(5ms, 5ms), TimestampList({5ms}));
    CHECK_BLOCK(lastBlock(), interval(4ms, 1ms), TimestampList({4ms, 1ms}));
}

TEST_F(TimelineDataListsCacheTest, addOverlappingEntirely)
{
    cache->add(interval(4ms, 3ms), kUnlimited, {4ms});
    cache->add(interval(9ms, 1ms), kUnlimited, {7ms, 4ms, 2ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(9ms, 1ms), TimestampList({7ms, 4ms, 2ms}));
}

TEST_F(TimelineDataListsCacheTest, addSamePeriod)
{
    cache->add(interval(9ms, 1ms), kUnlimited, {4ms});
    cache->add(interval(9ms, 1ms), kUnlimited, {7ms, 4ms, 2ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(9ms, 1ms), TimestampList({7ms, 4ms, 2ms}));
}

TEST_F(TimelineDataListsCacheTest, addOverlappingMiddle)
{
    cache->add(interval(9ms, 1ms), kUnlimited, {7ms, 4ms, 2ms});
    cache->add(interval(7ms, 3ms), kUnlimited, {7ms, 5ms, 4ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(3);
    EXPECT_CACHE_SIZE(4);
    auto it = cache->cache().begin();
    CHECK_BLOCK(*it, interval(9ms, 8ms), TimestampList({})); ++it;
    CHECK_BLOCK(*it, interval(2ms, 1ms), TimestampList({2ms})); ++it;
    CHECK_BLOCK(*it, interval(7ms, 3ms), TimestampList({7ms, 5ms, 4ms}));
}

TEST_F(TimelineDataListsCacheTest, addOverlappingComplexScenario)
{
    // 4 separate blocks.
    cache->add(interval(9ms, 7ms), kUnlimited, {9ms, 8ms, 8ms, 7ms});
    cache->add(interval(5ms, 5ms), kUnlimited, {5ms});
    cache->add(interval(4ms, 3ms), kUnlimited, {3ms, 3ms});
    cache->add(interval(2ms, 0ms), kUnlimited, {2ms, 1ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(4);
    EXPECT_CACHE_SIZE(9);

    // New block cuts blocks #1 and #4, blocks #2 and #3 get erased.
    cache->add(interval(8ms, 2ms), kUnlimited, {7ms, 5ms, 3ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(3);
    EXPECT_CACHE_SIZE(5);

    auto it = cache->cache().begin();
    CHECK_BLOCK(*it, interval(9ms, 9ms), TimestampList({9ms})); ++it;
    CHECK_BLOCK(*it, interval(1ms, 0ms), TimestampList({1ms})); ++it;
    CHECK_BLOCK(*it, interval(8ms, 2ms), TimestampList({7ms, 5ms, 3ms}));
}

// ------------------------------------------------------------------------------------------------
// Empty blocks (which are always complete) behavior.

// Not neighboring empty blocks are not merged.

TEST_F(TimelineDataListsCacheTest, addEmptySeparateLeft)
{
    cache->add(interval(2ms, 1ms), kUnlimited, {});
    cache->add(interval(9ms, 5ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(2ms, 1ms), TimestampList());
    CHECK_BLOCK(lastBlock(), interval(9ms, 5ms), TimestampList());
}

TEST_F(TimelineDataListsCacheTest, addEmptySeparateRight)
{
    cache->add(interval(9ms, 5ms), kUnlimited, {});
    cache->add(interval(2ms, 1ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(9ms, 5ms), TimestampList());
    CHECK_BLOCK(lastBlock(), interval(2ms, 1ms), TimestampList());
}

// Expirable empty blocks are not merged.

TEST_F(TimelineDataListsCacheTest, addEmptyExpirableTouchingLeft)
{
    cache->add(interval(2ms, 1ms), kUnlimited, {});
    cache->add(interval(5ms, 3ms), kUnlimited, {}, 30s);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(2ms, 1ms), TimestampList());
    CHECK_BLOCK(lastBlock(), interval(5ms, 3ms), TimestampList());
}

TEST_F(TimelineDataListsCacheTest, addEmptyExpirableTouchingRight)
{
    cache->add(interval(5ms, 3ms), kUnlimited, {});
    cache->add(interval(2ms, 1ms), kUnlimited, {}, 30s);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(5ms, 3ms), TimestampList());
    CHECK_BLOCK(lastBlock(), interval(2ms, 1ms), TimestampList());
}

TEST_F(TimelineDataListsCacheTest, addEmptyToExpirableTouchingLeft)
{
    cache->add(interval(2ms, 1ms), kUnlimited, {}, 30s);
    cache->add(interval(5ms, 3ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(2ms, 1ms), TimestampList());
    CHECK_BLOCK(lastBlock(), interval(5ms, 3ms), TimestampList());
}

TEST_F(TimelineDataListsCacheTest, addEmptyToExpirableTouchingRight)
{
    cache->add(interval(5ms, 3ms), kUnlimited, {}, 30s);
    cache->add(interval(2ms, 1ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(5ms, 3ms), TimestampList());
    CHECK_BLOCK(lastBlock(), interval(2ms, 1ms), TimestampList());
}

// Neighboring or overlapped not expirable empty blocks are merged.

TEST_F(TimelineDataListsCacheTest, appendEmptyTouchingLeft)
{
    cache->add(interval(2ms, 1ms), kUnlimited, {});
    cache->add(interval(5ms, 3ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(5ms, 1ms), TimestampList());
}

TEST_F(TimelineDataListsCacheTest, appendEmptyTouchingRight)
{
    cache->add(interval(5ms, 3ms), kUnlimited, {});
    cache->add(interval(2ms, 1ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(5ms, 1ms), TimestampList());
}

TEST_F(TimelineDataListsCacheTest, appendEmptyOverlappingLeft)
{
    cache->add(interval(4ms, 1ms), kUnlimited, {});
    cache->add(interval(5ms, 3ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(5ms, 1ms), TimestampList());
}

TEST_F(TimelineDataListsCacheTest, appendEmptyOverlappingRight)
{
    cache->add(interval(5ms, 3ms), kUnlimited, {});
    cache->add(interval(4ms, 1ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(5ms, 1ms), TimestampList());
}

TEST_F(TimelineDataListsCacheTest, appendEmptyOverlappingComplexScenario)
{
    // 4 separate blocks.
    cache->add(interval(9ms, 7ms), kUnlimited, {8ms, 8ms, 7ms});
    cache->add(interval(5ms, 5ms), kUnlimited, {5ms});
    cache->add(interval(4ms, 3ms), kUnlimited, {4ms, 3ms});
    cache->add(interval(2ms, 0ms), kUnlimited, {2ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(4);
    EXPECT_CACHE_SIZE(7);

    // New block cuts blocks #1 and #4 and they become empty; blocks #2 and #3 get erased.
    cache->add(interval(8ms, 2ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(0);
    CHECK_BLOCK(firstBlock(), interval(9ms, 0ms), TimestampList());
}

// ------------------------------------------------------------------------------------------------
// Incomplete blocks behavior.

TEST_F(TimelineDataListsCacheTest, addIncompleteOneMs)
{
    cache->add(interval(5ms, 1ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_EQ(cache->cacheSize(), 2);
    CHECK_INCOMPLETE_BLOCK(firstBlock(), 5ms, TimestampList({5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteAsTwoBlocks1)
{
    cache->add(interval(9ms, 1ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_EQ(cache->cacheSize(), 2);
    CHECK_INCOMPLETE_BLOCK(firstBlock(), 5ms, TimestampList({5ms, 5ms}));
    CHECK_BLOCK(lastBlock(), interval(9ms, 6ms), TimestampList());
}

TEST_F(TimelineDataListsCacheTest, addIncompleteAsTwoBlocks2)
{
    cache->add(interval(9ms, 1ms), /*limit*/ 3, {9ms, 6ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_EQ(cache->cacheSize(), 3);
    CHECK_INCOMPLETE_BLOCK(firstBlock(), 5ms, TimestampList({5ms}));
    CHECK_BLOCK(lastBlock(), interval(9ms, 6ms), TimestampList({9ms, 6ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteSeparateLeft)
{
    cache->add(interval(3ms, 1ms), kUnlimited, {3ms, 2ms});
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_EQ(cache->cacheSize(), 4);
    CHECK_BLOCK(firstBlock(), interval(3ms, 1ms), TimestampList({3ms, 2ms}));
    CHECK_INCOMPLETE_BLOCK(lastBlock(), 5ms, TimestampList({5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteSeparateRight)
{
    cache->add(interval(9ms, 7ms), kUnlimited, {8ms, 7ms});
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_EQ(cache->cacheSize(), 4);
    CHECK_BLOCK(firstBlock(), interval(9ms, 7ms), TimestampList({8ms, 7ms}));
    CHECK_INCOMPLETE_BLOCK(lastBlock(), 5ms, TimestampList({5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteTouchingLeft)
{
    cache->add(interval(4ms, 1ms), kUnlimited, {3ms, 2ms});
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_EQ(cache->cacheSize(), 4);
    CHECK_BLOCK(firstBlock(), interval(4ms, 1ms), TimestampList({3ms, 2ms}));
    CHECK_INCOMPLETE_BLOCK(lastBlock(), 5ms, TimestampList({5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteTouchingRight)
{
    cache->add(interval(9ms, 6ms), kUnlimited, {8ms, 7ms});
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_EQ(cache->cacheSize(), 4);
    CHECK_BLOCK(firstBlock(), interval(9ms, 6ms), TimestampList({8ms, 7ms}));
    CHECK_INCOMPLETE_BLOCK(lastBlock(), 5ms, TimestampList({5ms, 5ms}));
}

// In current implementation incoming blocks overwrite existing blocks even if they have less data.

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingLeft)
{
    cache->add(interval(5ms, 1ms), kUnlimited, {5ms, 5ms, 5ms, 5ms, 3ms, 2ms});
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_EQ(cache->cacheSize(), 4);
    CHECK_BLOCK(firstBlock(), interval(4ms, 1ms), TimestampList({3ms, 2ms}));
    CHECK_INCOMPLETE_BLOCK(lastBlock(), 5ms, TimestampList({5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingRight)
{
    cache->add(interval(9ms, 5ms), kUnlimited, {8ms, 7ms, 5ms, 5ms, 5ms, 5ms});
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_EQ(cache->cacheSize(), 4);
    CHECK_BLOCK(firstBlock(), interval(9ms, 6ms), TimestampList({8ms, 7ms}));
    CHECK_INCOMPLETE_BLOCK(lastBlock(), 5ms, TimestampList({5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingMiddle)
{
    cache->add(interval(9ms, 1ms), kUnlimited, {8ms, 7ms, 5ms, 5ms, 5ms, 5ms, 3ms, 2ms});
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(3);
    EXPECT_EQ(cache->cacheSize(), 6);

    auto it = cache->cache().begin();
    CHECK_BLOCK(*it, interval(9ms, 6ms), TimestampList({8ms, 7ms})); ++it;
    CHECK_BLOCK(*it, interval(4ms, 1ms), TimestampList({3ms, 2ms})); ++it;
    CHECK_INCOMPLETE_BLOCK(*it, 5ms, TimestampList({5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingIncomplete)
{
    cache->add(interval(5ms, 5ms), /*limit*/ 3, {5ms, 5ms, 5ms});
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_EQ(cache->cacheSize(), 2);
    CHECK_INCOMPLETE_BLOCK(lastBlock(), 5ms, TimestampList({5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteFullyOverlappingComplete)
{
    cache->add(interval(5ms, 5ms), kUnlimited, {5ms, 5ms, 5ms});
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_EQ(cache->cacheSize(), 2);
    CHECK_INCOMPLETE_BLOCK(lastBlock(), 5ms, TimestampList({5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addCompleteFullyOverlappingIncomplete)
{
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    cache->add(interval(5ms, 5ms), kUnlimited, {5ms, 5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_EQ(cache->cacheSize(), 3);
    CHECK_BLOCK(lastBlock(), interval(5ms, 5ms), TimestampList({5ms, 5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addCompleteOverlappingIncompleteMiddle)
{
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    cache->add(interval(9ms, 1ms), kUnlimited, {8ms, 6ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_EQ(cache->cacheSize(), 3);
    CHECK_BLOCK(lastBlock(), interval(9ms, 1ms), TimestampList({8ms, 6ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addCompleteOverlappingIncompleteLeft)
{
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    cache->add(interval(9ms, 5ms), kUnlimited, {8ms, 6ms, 5ms, 5ms, 5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_EQ(cache->cacheSize(), 5);
    CHECK_BLOCK(lastBlock(), interval(9ms, 5ms), TimestampList({8ms, 6ms, 5ms, 5ms, 5ms}));
}

TEST_F(TimelineDataListsCacheTest, addCompleteOverlappingIncompleteRight)
{
    cache->add(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    cache->add(interval(5ms, 1ms), kUnlimited, {5ms, 5ms, 5ms, 3ms, 1ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_EQ(cache->cacheSize(), 5);
    CHECK_BLOCK(lastBlock(), interval(5ms, 1ms), TimestampList({5ms, 5ms, 5ms, 3ms, 1ms}));
}

TEST_F(TimelineDataListsCacheTest, addIncompleteComplexScenario)
{
    // 4 separate blocks.
    cache->add(interval(9ms, 7ms), /*limit*/ 3, {9ms, 8ms, 7ms});
    cache->add(interval(5ms, 5ms), kUnlimited, {5ms});
    cache->add(interval(4ms, 3ms), kUnlimited, {3ms, 3ms});
    cache->add(interval(2ms, 0ms), kUnlimited, {2ms, 1ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(5); //< #1(c), #1(i), #2, #3, #4
    EXPECT_CACHE_SIZE(8);

    // New block cuts blocks #1(c) and #4, blocks #1(i), #2 and #3 get erased.
    // The new block is itself split into two parts, complete and incomplete.
    cache->add(interval(8ms, 2ms), /*limit*/ 4, {7ms, 5ms, 3ms, 2ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(4);
    EXPECT_CACHE_SIZE(6);

    auto it = cache->cache().begin();
    CHECK_BLOCK(*it, interval(9ms, 9ms), TimestampList({9ms})); ++it;
    CHECK_BLOCK(*it, interval(1ms, 0ms), TimestampList({1ms})); ++it;
    CHECK_INCOMPLETE_BLOCK(*it, 2ms, TimestampList({2ms})); ++it;
    CHECK_BLOCK(*it, interval(8ms, 3ms), TimestampList({7ms, 5ms, 3ms}));
}

// ------------------------------------------------------------------------------------------------
// Cache lookup.

TEST_F(TimelineDataListsCacheTest, failToGetFromEmpty)
{
    const auto result = cache->get(interval(2ms, 1ms), kUnlimited);
    EXPECT_EQ(result, std::nullopt);
}

TEST_F(TimelineDataListsCacheTest, getExact)
{
    cache->add(interval(5ms, 1ms), kUnlimited, {4ms, 2ms, 1ms, 1ms, 1ms});
    const auto result = cache->get(interval(5ms, 1ms), kUnlimited);
    EXPECT_EQ(result, TimestampList({4ms, 2ms, 1ms, 1ms, 1ms}));
}

TEST_F(TimelineDataListsCacheTest, getSubset)
{
    cache->add(interval(5ms, 1ms), kUnlimited, {4ms, 2ms, 1ms, 1ms, 1ms});
    const auto result = cache->get(interval(4ms, 2ms), kUnlimited);
    EXPECT_EQ(result, TimestampList({4ms, 2ms}));
}

TEST_F(TimelineDataListsCacheTest, getLimited)
{
    cache->add(interval(5ms, 1ms), kUnlimited, {4ms, 2ms, 1ms, 1ms, 1ms});
    const auto result = cache->get(interval(5ms, 0ms), /*limit*/ 3);
    EXPECT_EQ(result, TimestampList({4ms, 2ms, 1ms}));
}

TEST_F(TimelineDataListsCacheTest, getCombination)
{
    cache->add(interval(9ms, 6ms), kUnlimited, {8ms, 7ms, 6ms, 6ms, 6ms});
    cache->add(interval(5ms, 4ms), kUnlimited, {5ms, 4ms, 4ms});
    cache->add(interval(3ms, 3ms), kUnlimited, {});
    cache->add(interval(2ms, 1ms), /*limit*/ 2, {2ms, 1ms});
    const auto result = cache->get(interval(6ms, 0ms), /*limit*/ 8);
    EXPECT_EQ(result, TimestampList({6ms, 6ms, 6ms, 5ms, 4ms, 4ms, 2ms, 1ms}));
}

TEST_F(TimelineDataListsCacheTest, getEmpty)
{
    cache->add(interval(9ms, 1ms), kUnlimited, {});
    const auto result = cache->get(interval(5ms, 3ms), kUnlimited);
    EXPECT_EQ(result, TimestampList());
}

TEST_F(TimelineDataListsCacheTest, failToGetPartial)
{
    cache->add(interval(9ms, 6ms), /*limit*/ 5, {8ms, 7ms, 6ms, 6ms, 6ms});
    cache->add(interval(5ms, 4ms), kUnlimited, {5ms, 5ms, 4ms});
    cache->add(interval(2ms, 1ms), kUnlimited, {2ms, 1ms});

    // Cannot get cached but incomplete interval.
    auto result = cache->get(interval(9ms, 6ms), kUnlimited);
    EXPECT_EQ(result, std::nullopt);

    // There is a gap between [9ms...6ms] and [5ms...4ms] due to the first block incompleteness.
    result = cache->get(interval(9ms, 4ms), /*limit*/ 8);
    EXPECT_EQ(result, std::nullopt);

    // There is a one millisecond gap between [5ms...4ms] and [2ms...1ms].
    result = cache->get(interval(5ms, 1ms), /*limit*/ 5);
    EXPECT_EQ(result, std::nullopt);
}

// ------------------------------------------------------------------------------------------------
// Expiration by time.
// It's possible to add already expired blocks for faster tests.

TEST_F(TimelineDataListsCacheTest, expiration)
{
    cache->add(interval(9ms, 6ms), kUnlimited, {8ms, 7ms, 6ms}, /*expirationTime*/ 0s);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(3);

    // After an attempt to get data, the block will be discarded.
    const auto result = cache->get(interval(9ms, 6ms), kUnlimited);
    EXPECT_EQ(result, std::nullopt);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(0);
    EXPECT_CACHE_SIZE(0);
}

TEST_F(TimelineDataListsCacheTest, expirationOfOne)
{
    cache->add(interval(9ms, 6ms), kUnlimited, {8ms, 7ms, 6ms});
    cache->add(interval(3ms, 1ms), kUnlimited, {3ms, 2ms});
    cache->add(interval(5ms, 4ms), kUnlimited, {5ms}, /*expirationTime*/ 0s);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(3);
    EXPECT_CACHE_SIZE(6);

    // After an attempt to get data, the expired block will be discarded.
    const auto result = cache->get(interval(9ms, 1ms), kUnlimited);
    EXPECT_EQ(result, std::nullopt);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(5);
}

// When a new block is being added and it is overlapping an existing expired block,
// the expired block is not cut, but discarded.

TEST_F(TimelineDataListsCacheTest, expirationAddOverlappingLeft)
{
    cache->add(interval(4ms, 1ms), kUnlimited, {4ms, 1ms}, 0s);
    cache->add(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(2);
    CHECK_BLOCK(firstBlock(), interval(5ms, 3ms), TimestampList({5ms, 4ms}));
}

TEST_F(TimelineDataListsCacheTest, expirationAddOverlappingRight)
{
    cache->add(interval(5ms, 3ms), kUnlimited, {5ms, 4ms}, 0s);
    cache->add(interval(4ms, 1ms), kUnlimited, {4ms, 1ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(2);
    CHECK_BLOCK(firstBlock(), interval(4ms, 1ms), TimestampList({4ms, 1ms}));
}

TEST_F(TimelineDataListsCacheTest, expirationAddOverlappingMiddle)
{
    cache->add(interval(9ms, 1ms), kUnlimited, {7ms, 4ms, 2ms}, 0s);
    cache->add(interval(7ms, 3ms), kUnlimited, {7ms, 5ms, 4ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(7ms, 3ms), TimestampList({7ms, 5ms, 4ms}));
}

// ------------------------------------------------------------------------------------------------
// Other.

TEST_F(TimelineDataListsCacheTest, touchByGet)
{
    cache->add(interval(1ms, 1ms), kUnlimited, {1ms});
    cache->add(interval(3ms, 3ms), kUnlimited, {3ms});
    cache->add(interval(5ms, 5ms), kUnlimited, {5ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(3);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(1ms, 1ms), TimestampList({1ms}));
    CHECK_BLOCK(lastBlock(), interval(5ms, 5ms), TimestampList({5ms}));
    cache->get(interval(1ms, 1ms), kUnlimited);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(3);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(3ms, 3ms), TimestampList({3ms}));
    CHECK_BLOCK(lastBlock(), interval(1ms, 1ms), TimestampList({1ms}));
}

TEST_F(TimelineDataListsCacheTest, clear)
{
    cache->add(interval(2ms, 1ms), kUnlimited, {1ms});
    cache->clear();
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(0);
    EXPECT_CACHE_SIZE(0);
}

TEST_F(TimelineDataListsCacheTest, trimToBlockCount)
{
    cache->add(interval(1ms, 1ms), kUnlimited, {1ms});
    cache->add(interval(3ms, 3ms), kUnlimited, {3ms});
    cache->add(interval(5ms, 5ms), kUnlimited, {5ms});
    cache->setMaximumBlockCount(2);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(2);
    CHECK_BLOCK(firstBlock(), interval(3ms, 3ms), TimestampList({3ms}));
    CHECK_BLOCK(lastBlock(), interval(5ms, 5ms), TimestampList({5ms}));
}

TEST_F(TimelineDataListsCacheTest, trimToMaximumSize)
{
    cache->add(interval(1ms, 1ms), kUnlimited, {1ms, 1ms, 1ms});
    cache->add(interval(3ms, 3ms), kUnlimited, {3ms, 3ms});
    cache->add(interval(5ms, 5ms), kUnlimited, {5ms});
    cache->add(interval(6ms, 6ms), kUnlimited, {6ms, 6ms, 6ms});
    ASSERT_BLOCK_COUNT(4);

    cache->setMaximumCacheSize(7);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(3);
    EXPECT_CACHE_SIZE(6);
    CHECK_BLOCK(firstBlock(), interval(3ms, 3ms), TimestampList({3ms, 3ms}));

    cache->setMaximumCacheSize(6);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(3);
    EXPECT_CACHE_SIZE(6);
    CHECK_BLOCK(firstBlock(), interval(3ms, 3ms), TimestampList({3ms, 3ms}));

    cache->setMaximumCacheSize(3);
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(1);
    EXPECT_CACHE_SIZE(3);
    CHECK_BLOCK(firstBlock(), interval(6ms, 6ms), TimestampList({6ms, 6ms, 6ms}));
}

TEST_F(TimelineDataListsCacheTest, trimAfterAdd)
{
    cache->setMaximumBlockCount(3);
    cache->setMaximumCacheSize(5);

    cache->add(interval(9ms, 8ms), kUnlimited, {9ms, 8ms});
    cache->add(interval(7ms, 5ms), kUnlimited, {7ms, 5ms});
    ASSERT_BLOCK_COUNT(2);

    // Add another block, causing the total size exceeding `maximumCacheSize`.
    cache->add(interval(3ms, 2ms), kUnlimited, {3ms, 3ms});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(2);
    EXPECT_CACHE_SIZE(4);
    CHECK_BLOCK(firstBlock(), interval(7ms, 5ms), TimestampList({7ms, 5ms}));
    CHECK_BLOCK(lastBlock(), interval(3ms, 2ms), TimestampList({3ms, 3ms}));

    // Add two empty blocks, causing the number of blocks exceeding `maximumBlockCount`.
    cache->add(interval(15ms, 10ms), kUnlimited, {});
    cache->add(interval(1ms, 1ms), kUnlimited, {});
    CHECK_INTEGRITY;
    ASSERT_BLOCK_COUNT(3);
    EXPECT_CACHE_SIZE(2);
    CHECK_BLOCK(firstBlock(), interval(3ms, 2ms), TimestampList({3ms, 3ms}));
}

} // namespace test
} // namespace nx::vms::client::core::timeline
