// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include <nx/vms/client/core/timeline/backends/data_lists_cache.h>

#include "test_backends.h"

// clang-format off

namespace nx::vms::client::core::timeline {
namespace test {

using namespace std::chrono;

class TimelineDataListsCacheTest: public ::testing::Test
{
protected:
    using TimestampList = std::vector<milliseconds>;
    using Block = DataListsCache<TimestampList>::Block;

    struct ExpectedBlock
    {
        QnTimePeriod period;
        TimestampList data;
        bool complete = true;
    };

    static ExpectedBlock block(const QnTimePeriod& period, TimestampList data)
    {
        return {.period = period, .data = std::move(data), .complete = true};
    }

    static ExpectedBlock incompleteBlock(milliseconds timestamp, TimestampList data)
    {
        return {
            .period = QnTimePeriod(timestamp, 1ms), .data = std::move(data), .complete = false};
    }

    void givenBlock(const QnTimePeriod& period, int limit, TimestampList data,
        std::optional<seconds> expiration = std::nullopt)
    {
        cache.add(period, limit, std::move(data), expiration);
    }

    void givenInvalidated(const QnTimePeriod& period) { cache.invalidate(period); }
    void givenMaximumBlockCount(int value) { cache.setMaximumBlockCount(value); }
    void givenMaximumCacheSize(int value) { cache.setMaximumCacheSize(value); }

    void whenAdded(const QnTimePeriod& period, int limit, TimestampList data,
        std::optional<seconds> expiration = std::nullopt)
    {
        cache.add(period, limit, std::move(data), expiration);
    }

    void whenLookedUp(const QnTimePeriod& period, int limit) { result = cache.get(period, limit); }
    void whenInvalidated(const QnTimePeriod& period) { cache.invalidate(period); }
    void whenCleared() { cache.clear(); }
    void whenMaximumBlockCountSet(int value) { cache.setMaximumBlockCount(value); }
    void whenMaximumCacheSizeSetTo(int value) { cache.setMaximumCacheSize(value); }

    /** Checks the consistency of the internal structures. */
    void thenIntegrityIsKept()
    {
        int count = 0;
        EXPECT_EQ(cache.cache().size(), cache.lookup().size());
        for (auto blockIt = cache.cache().begin(); blockIt != cache.cache().end(); ++blockIt)
        {
            count += blockIt->size();
            const auto it = cache.lookup().find(blockIt->period().endTime());
            ASSERT_TRUE(it != cache.lookup().end());
            EXPECT_TRUE(it->second == blockIt);
            if (const auto nextIt = std::next(it); nextIt != cache.lookup().end())
                EXPECT_GE(blockIt->period().startTime(), nextIt->first);
        }
        EXPECT_EQ(count, cache.cacheSize());
    }

    void thenBlockCountIs(int expected) { ASSERT_EQ((int) cache.cache().size(), expected); }
    void thenCacheSizeIs(int expected) { EXPECT_EQ(cache.cacheSize(), expected); }

    /** Checks all blocks, from the least recently accessed to the most recently accessed. */
    void thenBlocksAre(const std::vector<ExpectedBlock>& expected)
    {
        ASSERT_EQ(cache.cache().size(), expected.size());
        auto it = cache.cache().begin();
        for (const auto& expectedBlock: expected)
            checkBlock(*it++, expectedBlock);
    }

    void thenGot(const TimestampList& expected) { EXPECT_EQ(result, expected); }
    void thenMissed() { EXPECT_EQ(result, std::nullopt); }

private:
    static void checkBlock(const Block& actual, const ExpectedBlock& expected)
    {
        EXPECT_EQ(actual.period(), expected.period);
        EXPECT_EQ(actual.data(), expected.data);
        EXPECT_EQ(actual.complete(), expected.complete);
    }

protected:
    DataListsCache<TimestampList> cache;
    std::optional<TimestampList> result;
};

// ------------------------------------------------------------------------------------------------
// Complete blocks behavior.

TEST_F(TimelineDataListsCacheTest, addComplete)
{
    whenAdded(interval(2ms, 1ms), kUnlimited, {1ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(1);
    thenBlocksAre({block(interval(2ms, 1ms), {1ms})});
}

TEST_F(TimelineDataListsCacheTest, addSeparateLeft)
{
    givenBlock(interval(2ms, 1ms), kUnlimited, {1ms});
    whenAdded(interval(9ms, 5ms), kUnlimited, {7ms, 6ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(3);
    thenBlocksAre({
        block(interval(2ms, 1ms), {1ms}),
        block(interval(9ms, 5ms), {7ms, 6ms})});
}

TEST_F(TimelineDataListsCacheTest, addSeparateRight)
{
    givenBlock(interval(9ms, 5ms), kUnlimited, {7ms, 6ms});
    whenAdded(interval(2ms, 1ms), kUnlimited, {1ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(3);
    thenBlocksAre({
        block(interval(9ms, 5ms), {7ms, 6ms}),
        block(interval(2ms, 1ms), {1ms})});
}

TEST_F(TimelineDataListsCacheTest, addTouchingLeft)
{
    givenBlock(interval(2ms, 1ms), kUnlimited, {1ms});
    whenAdded(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(3);
    thenBlocksAre({
        block(interval(2ms, 1ms), {1ms}),
        block(interval(5ms, 3ms), {5ms, 4ms})});
}

TEST_F(TimelineDataListsCacheTest, addTouchingRight)
{
    givenBlock(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    whenAdded(interval(2ms, 1ms), kUnlimited, {1ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(3);
    thenBlocksAre({
        block(interval(5ms, 3ms), {5ms, 4ms}),
        block(interval(2ms, 1ms), {1ms})});
}

TEST_F(TimelineDataListsCacheTest, addOverlappingLeft)
{
    givenBlock(interval(4ms, 1ms), kUnlimited, {4ms, 1ms});
    whenAdded(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(3);
    thenBlocksAre({
        block(interval(2ms, 1ms), {1ms}),
        block(interval(5ms, 3ms), {5ms, 4ms})});
}

TEST_F(TimelineDataListsCacheTest, addOverlappingRight)
{
    givenBlock(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    whenAdded(interval(4ms, 1ms), kUnlimited, {4ms, 1ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(3);
    thenBlocksAre({
        block(interval(5ms, 5ms), {5ms}),
        block(interval(4ms, 1ms), {4ms, 1ms})});
}

TEST_F(TimelineDataListsCacheTest, addOverlappingEntirely)
{
    givenBlock(interval(4ms, 3ms), kUnlimited, {4ms});
    whenAdded(interval(9ms, 1ms), kUnlimited, {7ms, 4ms, 2ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(3);
    thenBlocksAre({block(interval(9ms, 1ms), {7ms, 4ms, 2ms})});
}

TEST_F(TimelineDataListsCacheTest, addSamePeriod)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {4ms});
    whenAdded(interval(9ms, 1ms), kUnlimited, {7ms, 4ms, 2ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(3);
    thenBlocksAre({block(interval(9ms, 1ms), {7ms, 4ms, 2ms})});
}

TEST_F(TimelineDataListsCacheTest, addOverlappingMiddle)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {7ms, 4ms, 2ms});
    whenAdded(interval(7ms, 3ms), kUnlimited, {7ms, 5ms, 4ms});
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(4);
    thenBlocksAre({
        block(interval(9ms, 8ms), {}),
        block(interval(2ms, 1ms), {2ms}),
        block(interval(7ms, 3ms), {7ms, 5ms, 4ms})});
}

TEST_F(TimelineDataListsCacheTest, addOverlappingComplexScenario)
{
    // 4 separate blocks.
    givenBlock(interval(9ms, 7ms), kUnlimited, {9ms, 8ms, 8ms, 7ms});
    givenBlock(interval(5ms, 5ms), kUnlimited, {5ms});
    givenBlock(interval(4ms, 3ms), kUnlimited, {3ms, 3ms});
    givenBlock(interval(2ms, 0ms), kUnlimited, {2ms, 1ms});

    // New block cuts blocks #1 and #4, blocks #2 and #3 get erased.
    whenAdded(interval(8ms, 2ms), kUnlimited, {7ms, 5ms, 3ms});
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(5);
    thenBlocksAre({
        block(interval(9ms, 9ms), {9ms}),
        block(interval(1ms, 0ms), {1ms}),
        block(interval(8ms, 2ms), {7ms, 5ms, 3ms})});
}

// ------------------------------------------------------------------------------------------------
// Empty blocks (which are always complete) behavior.

// Not neighboring empty blocks are not merged.

TEST_F(TimelineDataListsCacheTest, addEmptySeparateLeft)
{
    givenBlock(interval(2ms, 1ms), kUnlimited, {});
    whenAdded(interval(9ms, 5ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(0);
    thenBlocksAre({
        block(interval(2ms, 1ms), {}),
        block(interval(9ms, 5ms), {})});
}

TEST_F(TimelineDataListsCacheTest, addEmptySeparateRight)
{
    givenBlock(interval(9ms, 5ms), kUnlimited, {});
    whenAdded(interval(2ms, 1ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(0);
    thenBlocksAre({
        block(interval(9ms, 5ms), {}),
        block(interval(2ms, 1ms), {})});
}

// Expirable empty blocks are not merged.

TEST_F(TimelineDataListsCacheTest, addEmptyExpirableTouchingLeft)
{
    givenBlock(interval(2ms, 1ms), kUnlimited, {});
    whenAdded(interval(5ms, 3ms), kUnlimited, {}, 30s);
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(0);
    thenBlocksAre({
        block(interval(2ms, 1ms), {}),
        block(interval(5ms, 3ms), {})});
}

TEST_F(TimelineDataListsCacheTest, addEmptyExpirableTouchingRight)
{
    givenBlock(interval(5ms, 3ms), kUnlimited, {});
    whenAdded(interval(2ms, 1ms), kUnlimited, {}, 30s);
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(0);
    thenBlocksAre({
        block(interval(5ms, 3ms), {}),
        block(interval(2ms, 1ms), {})});
}

TEST_F(TimelineDataListsCacheTest, addEmptyToExpirableTouchingLeft)
{
    givenBlock(interval(2ms, 1ms), kUnlimited, {}, 30s);
    whenAdded(interval(5ms, 3ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(0);
    thenBlocksAre({
        block(interval(2ms, 1ms), {}),
        block(interval(5ms, 3ms), {})});
}

TEST_F(TimelineDataListsCacheTest, addEmptyToExpirableTouchingRight)
{
    givenBlock(interval(5ms, 3ms), kUnlimited, {}, 30s);
    whenAdded(interval(2ms, 1ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(0);
    thenBlocksAre({
        block(interval(5ms, 3ms), {}),
        block(interval(2ms, 1ms), {})});
}

// Neighboring or overlapped not expirable empty blocks are merged.

TEST_F(TimelineDataListsCacheTest, appendEmptyTouchingLeft)
{
    givenBlock(interval(2ms, 1ms), kUnlimited, {});
    whenAdded(interval(5ms, 3ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(0);
    thenBlocksAre({block(interval(5ms, 1ms), {})});
}

TEST_F(TimelineDataListsCacheTest, appendEmptyTouchingRight)
{
    givenBlock(interval(5ms, 3ms), kUnlimited, {});
    whenAdded(interval(2ms, 1ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(0);
    thenBlocksAre({block(interval(5ms, 1ms), {})});
}

TEST_F(TimelineDataListsCacheTest, appendEmptyOverlappingLeft)
{
    givenBlock(interval(4ms, 1ms), kUnlimited, {});
    whenAdded(interval(5ms, 3ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(0);
    thenBlocksAre({block(interval(5ms, 1ms), {})});
}

TEST_F(TimelineDataListsCacheTest, appendEmptyOverlappingRight)
{
    givenBlock(interval(5ms, 3ms), kUnlimited, {});
    whenAdded(interval(4ms, 1ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(0);
    thenBlocksAre({block(interval(5ms, 1ms), {})});
}

TEST_F(TimelineDataListsCacheTest, appendEmptyOverlappingComplexScenario)
{
    // 4 separate blocks.
    givenBlock(interval(9ms, 7ms), kUnlimited, {8ms, 8ms, 7ms});
    givenBlock(interval(5ms, 5ms), kUnlimited, {5ms});
    givenBlock(interval(4ms, 3ms), kUnlimited, {4ms, 3ms});
    givenBlock(interval(2ms, 0ms), kUnlimited, {2ms});

    // New block cuts blocks #1 and #4 and they become empty; blocks #2 and #3 get erased.
    whenAdded(interval(8ms, 2ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(0);
    thenBlocksAre({block(interval(9ms, 0ms), {})});
}

// ------------------------------------------------------------------------------------------------
// Incomplete blocks behavior.

TEST_F(TimelineDataListsCacheTest, addIncompleteOneMs)
{
    whenAdded(interval(5ms, 1ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({incompleteBlock(5ms, {5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteAsTwoBlocks1)
{
    whenAdded(interval(9ms, 1ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(2);
    thenBlocksAre({
        incompleteBlock(5ms, {5ms, 5ms}),
        block(interval(9ms, 6ms), {})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteAsTwoBlocks2)
{
    whenAdded(interval(9ms, 1ms), /*limit*/ 3, {9ms, 6ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(3);
    thenBlocksAre({
        incompleteBlock(5ms, {5ms}),
        block(interval(9ms, 6ms), {9ms, 6ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteSeparateLeft)
{
    givenBlock(interval(3ms, 1ms), kUnlimited, {3ms, 2ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(4);
    thenBlocksAre({
        block(interval(3ms, 1ms), {3ms, 2ms}),
        incompleteBlock(5ms, {5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteSeparateRight)
{
    givenBlock(interval(9ms, 7ms), kUnlimited, {8ms, 7ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(4);
    thenBlocksAre({
        block(interval(9ms, 7ms), {8ms, 7ms}),
        incompleteBlock(5ms, {5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteTouchingLeft)
{
    givenBlock(interval(4ms, 1ms), kUnlimited, {3ms, 2ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(4);
    thenBlocksAre({
        block(interval(4ms, 1ms), {3ms, 2ms}),
        incompleteBlock(5ms, {5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteTouchingRight)
{
    givenBlock(interval(9ms, 6ms), kUnlimited, {8ms, 7ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(4);
    thenBlocksAre({
        block(interval(9ms, 6ms), {8ms, 7ms}),
        incompleteBlock(5ms, {5ms, 5ms})});
}

// An incoming incomplete block doesn't overwrite an unexpired complete block.

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingLeft)
{
    givenBlock(interval(5ms, 1ms), kUnlimited, {5ms, 5ms, 5ms, 5ms, 3ms, 2ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(6);
    thenBlocksAre({block(interval(5ms, 1ms), {5ms, 5ms, 5ms, 5ms, 3ms, 2ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingRight)
{
    givenBlock(interval(9ms, 5ms), kUnlimited, {8ms, 7ms, 5ms, 5ms, 5ms, 5ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(6);
    thenBlocksAre({block(interval(9ms, 5ms), {8ms, 7ms, 5ms, 5ms, 5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingMiddle)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {8ms, 7ms, 5ms, 5ms, 5ms, 5ms, 3ms, 2ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(8);
    thenBlocksAre({block(interval(9ms, 1ms), {8ms, 7ms, 5ms, 5ms, 5ms, 5ms, 3ms, 2ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingExpiredComplete)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {8ms, 5ms, 5ms, 5ms, 2ms}, /*expiration*/ 0s);
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({incompleteBlock(5ms, {5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingCompleteWithSameCount)
{
    givenBlock(interval(5ms, 5ms), kUnlimited, {5ms, 5ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({block(interval(5ms, 5ms), {5ms, 5ms})});
}

// An incoming incomplete block overwrites a complete block which has fewer items in its
// millisecond, as it's outdated.

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingCompleteWithFewerItems)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {8ms, 5ms, 2ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(4);
    thenBlocksAre({
        block(interval(9ms, 6ms), {8ms}),
        block(interval(4ms, 1ms), {2ms}),
        incompleteBlock(5ms, {5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addTruncatedOverOutdatedEmpty)
{
    givenBlock(interval(9ms, 0ms), kUnlimited, {});

    // The incomplete part splits the outdated empty block, and the complete part replaces its
    // later part.
    whenAdded(interval(19ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(2);
    thenBlocksAre({
        block(interval(4ms, 0ms), {}),
        incompleteBlock(5ms, {5ms, 5ms}),
        block(interval(19ms, 6ms), {})});
}

TEST_F(TimelineDataListsCacheTest, addTruncatedOverComplete)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {8ms, 7ms, 5ms, 5ms, 3ms});

    // The incomplete millisecond 5ms is kept from the existing block.
    whenAdded(interval(9ms, 5ms), /*limit*/ 3, {8ms, 7ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(5);
    thenBlocksAre({
        block(interval(5ms, 1ms), {5ms, 5ms, 3ms}),
        block(interval(9ms, 6ms), {8ms, 7ms})});
}

TEST_F(TimelineDataListsCacheTest, getAfterTruncatedOverComplete)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {8ms, 7ms, 5ms, 5ms, 3ms});
    givenBlock(interval(9ms, 5ms), /*limit*/ 3, {8ms, 7ms, 5ms});
    whenLookedUp(interval(9ms, 1ms), kUnlimited);
    thenGot({8ms, 7ms, 5ms, 5ms, 3ms});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteOverlappingIncomplete)
{
    givenBlock(interval(5ms, 5ms), /*limit*/ 3, {5ms, 5ms, 5ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({incompleteBlock(5ms, {5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteFullyOverlappingComplete)
{
    givenBlock(interval(5ms, 5ms), kUnlimited, {5ms, 5ms, 5ms});
    whenAdded(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(3);
    thenBlocksAre({block(interval(5ms, 5ms), {5ms, 5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addCompleteFullyOverlappingIncomplete)
{
    givenBlock(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    whenAdded(interval(5ms, 5ms), kUnlimited, {5ms, 5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(3);
    thenBlocksAre({block(interval(5ms, 5ms), {5ms, 5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addCompleteOverlappingIncompleteMiddle)
{
    givenBlock(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    whenAdded(interval(9ms, 1ms), kUnlimited, {8ms, 6ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(3);
    thenBlocksAre({block(interval(9ms, 1ms), {8ms, 6ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addCompleteOverlappingIncompleteLeft)
{
    givenBlock(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    whenAdded(interval(9ms, 5ms), kUnlimited, {8ms, 6ms, 5ms, 5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(5);
    thenBlocksAre({block(interval(9ms, 5ms), {8ms, 6ms, 5ms, 5ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, addCompleteOverlappingIncompleteRight)
{
    givenBlock(interval(5ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    whenAdded(interval(5ms, 1ms), kUnlimited, {5ms, 5ms, 5ms, 3ms, 1ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(5);
    thenBlocksAre({block(interval(5ms, 1ms), {5ms, 5ms, 5ms, 3ms, 1ms})});
}

TEST_F(TimelineDataListsCacheTest, addIncompleteComplexScenario)
{
    // 4 separate blocks; #1 is incomplete, so it's stored as complete #1(c) and incomplete #1(i).
    givenBlock(interval(9ms, 7ms), /*limit*/ 3, {9ms, 8ms, 7ms});
    givenBlock(interval(5ms, 5ms), kUnlimited, {5ms});
    givenBlock(interval(4ms, 3ms), kUnlimited, {3ms, 3ms});
    givenBlock(interval(2ms, 0ms), kUnlimited, {2ms, 1ms});

    // New block is split into complete [8ms...3ms] and incomplete [2ms]. The complete part cuts
    // block #1(c) and erases blocks #1(i), #2 and #3; the incomplete part is dropped, as complete
    // block #4 contains 2ms.
    whenAdded(interval(8ms, 2ms), /*limit*/ 4, {7ms, 5ms, 3ms, 2ms});
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(6);
    thenBlocksAre({
        block(interval(9ms, 9ms), {9ms}),
        block(interval(2ms, 0ms), {2ms, 1ms}),
        block(interval(8ms, 3ms), {7ms, 5ms, 3ms})});
}

// ------------------------------------------------------------------------------------------------
// Cache lookup.

TEST_F(TimelineDataListsCacheTest, failToGetFromEmpty)
{
    whenLookedUp(interval(2ms, 1ms), kUnlimited);
    thenMissed();
    thenIntegrityIsKept();
    thenBlockCountIs(0);
    thenCacheSizeIs(0);
}

TEST_F(TimelineDataListsCacheTest, getExact)
{
    givenBlock(interval(5ms, 1ms), kUnlimited, {4ms, 2ms, 1ms, 1ms, 1ms});
    whenLookedUp(interval(5ms, 1ms), kUnlimited);
    thenGot({4ms, 2ms, 1ms, 1ms, 1ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(5);
}

TEST_F(TimelineDataListsCacheTest, getSubset)
{
    givenBlock(interval(5ms, 1ms), kUnlimited, {4ms, 2ms, 1ms, 1ms, 1ms});
    whenLookedUp(interval(4ms, 2ms), kUnlimited);
    thenGot({4ms, 2ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(5);
}

TEST_F(TimelineDataListsCacheTest, getLimited)
{
    givenBlock(interval(5ms, 1ms), kUnlimited, {4ms, 2ms, 1ms, 1ms, 1ms});
    whenLookedUp(interval(5ms, 0ms), /*limit*/ 3);
    thenGot({4ms, 2ms, 1ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(5);
}

TEST_F(TimelineDataListsCacheTest, getCombination)
{
    givenBlock(interval(9ms, 6ms), kUnlimited, {8ms, 7ms, 6ms, 6ms, 6ms});
    givenBlock(interval(5ms, 4ms), kUnlimited, {5ms, 4ms, 4ms});
    givenBlock(interval(3ms, 3ms), kUnlimited, {});
    givenBlock(interval(2ms, 1ms), /*limit*/ 2, {2ms, 1ms});
    whenLookedUp(interval(6ms, 0ms), /*limit*/ 8);
    thenGot({6ms, 6ms, 6ms, 5ms, 4ms, 4ms, 2ms, 1ms});
    thenIntegrityIsKept();
    thenBlockCountIs(5);
    thenCacheSizeIs(10);
}

TEST_F(TimelineDataListsCacheTest, getEmpty)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {});
    whenLookedUp(interval(5ms, 3ms), kUnlimited);
    thenGot({});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(0);
}

TEST_F(TimelineDataListsCacheTest, failToGetIncomplete)
{
    givenBlock(interval(9ms, 6ms), /*limit*/ 5, {8ms, 7ms, 6ms, 6ms, 6ms});
    givenBlock(interval(5ms, 4ms), kUnlimited, {5ms, 5ms, 4ms});
    givenBlock(interval(2ms, 1ms), kUnlimited, {2ms, 1ms});

    // Cannot get cached but incomplete interval.
    whenLookedUp(interval(9ms, 6ms), kUnlimited);
    thenMissed();
    thenIntegrityIsKept();
    thenBlockCountIs(4);
    thenCacheSizeIs(10);
}

TEST_F(TimelineDataListsCacheTest, failToGetPastIncomplete)
{
    givenBlock(interval(9ms, 6ms), /*limit*/ 5, {8ms, 7ms, 6ms, 6ms, 6ms});
    givenBlock(interval(5ms, 4ms), kUnlimited, {5ms, 5ms, 4ms});
    givenBlock(interval(2ms, 1ms), kUnlimited, {2ms, 1ms});

    // There is a gap between [9ms...6ms] and [5ms...4ms] due to the first block incompleteness.
    whenLookedUp(interval(9ms, 4ms), /*limit*/ 8);
    thenMissed();
    thenIntegrityIsKept();
    thenBlockCountIs(4);
    thenCacheSizeIs(10);
}

TEST_F(TimelineDataListsCacheTest, failToGetOverGap)
{
    givenBlock(interval(9ms, 6ms), /*limit*/ 5, {8ms, 7ms, 6ms, 6ms, 6ms});
    givenBlock(interval(5ms, 4ms), kUnlimited, {5ms, 5ms, 4ms});
    givenBlock(interval(2ms, 1ms), kUnlimited, {2ms, 1ms});

    // There is a one millisecond gap between [5ms...4ms] and [2ms...1ms].
    whenLookedUp(interval(5ms, 1ms), /*limit*/ 5);
    thenMissed();
    thenIntegrityIsKept();
    thenBlockCountIs(4);
    thenCacheSizeIs(10);
}

TEST_F(TimelineDataListsCacheTest, failToGetStartingWithIncompleteBlock)
{
    givenBlock(interval(4ms, 0ms), kUnlimited, {2ms});
    givenBlock(interval(9ms, 5ms), /*limit*/ 2, {5ms, 5ms});

    // The lookup starts with the incomplete block [5ms...5ms], which can't be walked past, as some
    // items of that millisecond may be missing.
    whenLookedUp(interval(5ms, 0ms), /*limit*/ 5);
    thenMissed();
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(3);
}

TEST_F(TimelineDataListsCacheTest, getWithinIncompleteBlock)
{
    givenBlock(interval(4ms, 0ms), kUnlimited, {2ms});
    givenBlock(interval(9ms, 5ms), /*limit*/ 2, {5ms, 5ms});

    // The limit can be filled up within the incomplete block.
    whenLookedUp(interval(5ms, 0ms), /*limit*/ 2);
    thenGot({5ms, 5ms});
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(3);
}

TEST_F(TimelineDataListsCacheTest, failToGetPastIncompleteBlockOverOutdatedEmpty)
{
    givenBlock(interval(9ms, 0ms), kUnlimited, {});
    givenBlock(interval(19ms, 5ms), /*limit*/ 2, {5ms, 5ms});

    // The outdated empty block doesn't hide the items received later.
    whenLookedUp(interval(19ms, 0ms), kUnlimited);
    thenMissed();
}

TEST_F(TimelineDataListsCacheTest, getIncompleteBlockOverOutdatedEmpty)
{
    givenBlock(interval(9ms, 0ms), kUnlimited, {});
    givenBlock(interval(19ms, 5ms), /*limit*/ 2, {5ms, 5ms});
    whenLookedUp(interval(19ms, 0ms), /*limit*/ 2);
    thenGot({5ms, 5ms});
}

// ------------------------------------------------------------------------------------------------
// Expiration by time.
// It's possible to add already expired blocks for faster tests.

TEST_F(TimelineDataListsCacheTest, expiration)
{
    givenBlock(interval(9ms, 6ms), kUnlimited, {8ms, 7ms, 6ms}, /*expiration*/ 0s);

    // After an attempt to get data, the block will be discarded.
    whenLookedUp(interval(9ms, 6ms), kUnlimited);
    thenMissed();
    thenIntegrityIsKept();
    thenBlockCountIs(0);
    thenCacheSizeIs(0);
}

TEST_F(TimelineDataListsCacheTest, expirationOfOne)
{
    givenBlock(interval(9ms, 6ms), kUnlimited, {8ms, 7ms, 6ms});
    givenBlock(interval(3ms, 1ms), kUnlimited, {3ms, 2ms});
    givenBlock(interval(5ms, 4ms), kUnlimited, {5ms}, /*expiration*/ 0s);

    // After an attempt to get data, the expired block will be discarded.
    whenLookedUp(interval(9ms, 1ms), kUnlimited);
    thenMissed();
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(5);
}

// When a new block is being added and it is overlapping an existing expired block,
// the expired block is not cut, but discarded.

TEST_F(TimelineDataListsCacheTest, expirationAddOverlappingLeft)
{
    givenBlock(interval(4ms, 1ms), kUnlimited, {4ms, 1ms}, /*expiration*/ 0s);
    whenAdded(interval(5ms, 3ms), kUnlimited, {5ms, 4ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({block(interval(5ms, 3ms), {5ms, 4ms})});
}

TEST_F(TimelineDataListsCacheTest, expirationAddOverlappingRight)
{
    givenBlock(interval(5ms, 3ms), kUnlimited, {5ms, 4ms}, /*expiration*/ 0s);
    whenAdded(interval(4ms, 1ms), kUnlimited, {4ms, 1ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({block(interval(4ms, 1ms), {4ms, 1ms})});
}

TEST_F(TimelineDataListsCacheTest, expirationAddOverlappingMiddle)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {7ms, 4ms, 2ms}, /*expiration*/ 0s);
    whenAdded(interval(7ms, 3ms), kUnlimited, {7ms, 5ms, 4ms});
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(3);
    thenBlocksAre({block(interval(7ms, 3ms), {7ms, 5ms, 4ms})});
}

// ------------------------------------------------------------------------------------------------
// Instant invalidation of a time period.

TEST_F(TimelineDataListsCacheTest, invalidateEmptyCache)
{
    whenInvalidated(interval(9ms, 1ms));
    thenIntegrityIsKept();
    thenBlockCountIs(0);
    thenCacheSizeIs(0);
}

TEST_F(TimelineDataListsCacheTest, invalidateNotIntersecting)
{
    givenBlock(interval(9ms, 5ms), kUnlimited, {9ms, 5ms});
    whenInvalidated(interval(3ms, 1ms));
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({block(interval(9ms, 5ms), {9ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, invalidateTouching)
{
    givenBlock(interval(9ms, 5ms), kUnlimited, {9ms, 5ms});
    whenInvalidated(interval(4ms, 1ms));
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({block(interval(9ms, 5ms), {9ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, invalidateEntireBlock)
{
    givenBlock(interval(9ms, 5ms), kUnlimited, {9ms, 5ms});
    givenBlock(interval(4ms, 1ms), kUnlimited, {4ms, 1ms});
    whenInvalidated(interval(9ms, 5ms));
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({block(interval(4ms, 1ms), {4ms, 1ms})});
}

TEST_F(TimelineDataListsCacheTest, invalidateSeveralBlocks)
{
    givenBlock(interval(9ms, 7ms), kUnlimited, {9ms, 7ms});
    givenBlock(interval(6ms, 5ms), kUnlimited, {5ms});
    givenBlock(interval(4ms, 1ms), kUnlimited, {4ms, 1ms});
    whenInvalidated(interval(9ms, 5ms));
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({block(interval(4ms, 1ms), {4ms, 1ms})});
}

TEST_F(TimelineDataListsCacheTest, invalidateEarlierPartOfBlock)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {9ms, 5ms, 1ms});
    whenInvalidated(interval(4ms, 1ms));
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(2);
    thenBlocksAre({block(interval(9ms, 5ms), {9ms, 5ms})});
}

TEST_F(TimelineDataListsCacheTest, invalidateLaterPartOfBlock)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {9ms, 5ms, 1ms});
    whenInvalidated(interval(9ms, 5ms));
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(1);
    thenBlocksAre({block(interval(4ms, 1ms), {1ms})});
}

TEST_F(TimelineDataListsCacheTest, invalidateMiddleOfBlock)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {9ms, 5ms, 1ms});
    whenInvalidated(interval(6ms, 4ms));
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(2);
    thenBlocksAre({
        block(interval(9ms, 7ms), {9ms}),
        block(interval(3ms, 1ms), {1ms})});
}

TEST_F(TimelineDataListsCacheTest, invalidateIncompleteBlock)
{
    givenBlock(interval(5ms, 1ms), /*limit*/ 2, {5ms, 5ms});
    whenInvalidated(interval(5ms, 5ms));
    thenIntegrityIsKept();
    thenBlockCountIs(0);
    thenCacheSizeIs(0);
}

TEST_F(TimelineDataListsCacheTest, invalidateEverything)
{
    givenBlock(interval(9ms, 7ms), kUnlimited, {9ms, 7ms});
    givenBlock(interval(4ms, 1ms), kUnlimited, {4ms, 1ms});
    whenInvalidated(QnTimePeriod::anytime());
    thenIntegrityIsKept();
    thenBlockCountIs(0);
    thenCacheSizeIs(0);
}

TEST_F(TimelineDataListsCacheTest, invalidatedPeriodIsNotReturnedByGet)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {9ms, 5ms, 1ms});
    givenInvalidated(interval(6ms, 4ms));

    // The invalidated period is not in the cache anymore, thus the whole period is not covered.
    whenLookedUp(interval(9ms, 1ms), kUnlimited);
    thenMissed();
}

TEST_F(TimelineDataListsCacheTest, periodAfterInvalidatedIsReturnedByGet)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {9ms, 5ms, 1ms});
    givenInvalidated(interval(6ms, 4ms));

    // The remaining parts are still cached.
    whenLookedUp(interval(9ms, 7ms), kUnlimited);
    thenGot({9ms});
}

TEST_F(TimelineDataListsCacheTest, periodBeforeInvalidatedIsReturnedByGet)
{
    givenBlock(interval(9ms, 1ms), kUnlimited, {9ms, 5ms, 1ms});
    givenInvalidated(interval(6ms, 4ms));

    // The remaining parts are still cached.
    whenLookedUp(interval(3ms, 1ms), kUnlimited);
    thenGot({1ms});
}

// ------------------------------------------------------------------------------------------------
// Other.

TEST_F(TimelineDataListsCacheTest, touchByGet)
{
    givenBlock(interval(1ms, 1ms), kUnlimited, {1ms});
    givenBlock(interval(3ms, 3ms), kUnlimited, {3ms});
    givenBlock(interval(5ms, 5ms), kUnlimited, {5ms});

    whenLookedUp(interval(3ms, 3ms), kUnlimited);

    // Only the looked up block becomes the most recently accessed; others keep their order.
    thenBlocksAre({
        block(interval(1ms, 1ms), {1ms}),
        block(interval(5ms, 5ms), {5ms}),
        block(interval(3ms, 3ms), {3ms})});
}

TEST_F(TimelineDataListsCacheTest, clear)
{
    givenBlock(interval(2ms, 1ms), kUnlimited, {1ms});
    whenCleared();
    thenIntegrityIsKept();
    thenBlockCountIs(0);
    thenCacheSizeIs(0);
}

TEST_F(TimelineDataListsCacheTest, trimToBlockCount)
{
    givenBlock(interval(1ms, 1ms), kUnlimited, {1ms});
    givenBlock(interval(3ms, 3ms), kUnlimited, {3ms});
    givenBlock(interval(5ms, 5ms), kUnlimited, {5ms});
    whenMaximumBlockCountSet(2);
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(2);
    thenBlocksAre({
        block(interval(3ms, 3ms), {3ms}),
        block(interval(5ms, 5ms), {5ms})});
}

TEST_F(TimelineDataListsCacheTest, trimToMaximumSize)
{
    givenBlock(interval(1ms, 1ms), kUnlimited, {1ms, 1ms, 1ms});
    givenBlock(interval(3ms, 3ms), kUnlimited, {3ms, 3ms});
    givenBlock(interval(5ms, 5ms), kUnlimited, {5ms});
    givenBlock(interval(6ms, 6ms), kUnlimited, {6ms, 6ms, 6ms});

    whenMaximumCacheSizeSetTo(7);
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(6);
    thenBlocksAre({
        block(interval(3ms, 3ms), {3ms, 3ms}),
        block(interval(5ms, 5ms), {5ms}),
        block(interval(6ms, 6ms), {6ms, 6ms, 6ms})});

    whenMaximumCacheSizeSetTo(6);
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(6);
    thenBlocksAre({
        block(interval(3ms, 3ms), {3ms, 3ms}),
        block(interval(5ms, 5ms), {5ms}),
        block(interval(6ms, 6ms), {6ms, 6ms, 6ms})});

    whenMaximumCacheSizeSetTo(3);
    thenIntegrityIsKept();
    thenBlockCountIs(1);
    thenCacheSizeIs(3);
    thenBlocksAre({block(interval(6ms, 6ms), {6ms, 6ms, 6ms})});
}

TEST_F(TimelineDataListsCacheTest, trimAfterAdd)
{
    givenMaximumBlockCount(3);
    givenMaximumCacheSize(5);
    givenBlock(interval(9ms, 8ms), kUnlimited, {9ms, 8ms});
    givenBlock(interval(7ms, 5ms), kUnlimited, {7ms, 5ms});

    // Add another block, causing the total size exceeding `maximumCacheSize`.
    whenAdded(interval(3ms, 2ms), kUnlimited, {3ms, 3ms});
    thenIntegrityIsKept();
    thenBlockCountIs(2);
    thenCacheSizeIs(4);
    thenBlocksAre({
        block(interval(7ms, 5ms), {7ms, 5ms}),
        block(interval(3ms, 2ms), {3ms, 3ms})});

    // Add two empty blocks, causing the number of blocks exceeding `maximumBlockCount`.
    whenAdded(interval(15ms, 10ms), kUnlimited, {});
    whenAdded(interval(1ms, 1ms), kUnlimited, {});
    thenIntegrityIsKept();
    thenBlockCountIs(3);
    thenCacheSizeIs(2);
    thenBlocksAre({
        block(interval(3ms, 2ms), {3ms, 3ms}),
        block(interval(15ms, 10ms), {}),
        block(interval(1ms, 1ms), {})});
}

} // namespace test
} // namespace nx::vms::client::core::timeline
