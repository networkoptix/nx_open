// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>

#include "structures.h"

namespace {

using namespace std::chrono;
using namespace nx::vms::client::core;
using namespace nx::vms::client::core::event_search;
using FetchDirection = EventSearch::FetchDirection;

using TestItems = std::vector<Item>;

// Default sort order for the "current items" is descending by the start time.
static const TestItems kCurrentItems = {
    Item::create(100ms, "first_current_item"),
    Item::create(90ms, "second_current_item"),
    Item::create(60ms, "penlast_current_item"),
    Item::create(50ms, "last_current_item")
};

static const auto kAboveItem = Item::create(milliseconds(110), "above_all_current");
static const auto kBelowItem = Item::create(milliseconds(40), "below_all_current");

template<typename Accessor, typename CurrentContainer, typename FetchedContainer>
FetchedData<FetchedContainer> makeData(
    const CurrentContainer& current,
    FetchedContainer& fetched,
    const FetchDirection& direction)
{
    const auto request =
        [current, direction]()
        {
            static const auto extractCentralPointUs =
                [](const auto& iterator)
            {
                const auto timestamp = Accessor::startTime(*iterator);
                return duration_cast<microseconds>(timestamp);
            };

            const auto timePointUs = direction == FetchDirection::older
                ? extractCentralPointUs(current.rbegin())
                : extractCentralPointUs(current.begin());
            return FetchRequest{.direction = direction, .centralPointUs = timePointUs};
        }();

    return makeFetchedData<Accessor>(current, fetched, request);
}

} // namespace

/**
 * We expect to have only body if fetched data is the same as a current one.
 */
TEST(FetchedData, fetchedTheSameData)
{
    // Older request check.
    {
        auto fetched = kCurrentItems;
        const auto result = makeData<Accessor>(kCurrentItems, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, kCurrentItems.size());
        ASSERT_EQ(result.ranges.tail.length, 0);
    }

    // Newer request check.
    {
        auto fetched = TestItems(kCurrentItems.rbegin(), kCurrentItems.rend());
        const auto result = makeData<Accessor>(kCurrentItems, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, kCurrentItems.size());
        ASSERT_EQ(result.ranges.tail.length, 0);
    }
}

/**
 * Checking cases when fetched data fully out of the bound from the each side of current data.
 */
TEST(FetchedData, fetchedDataIsOutOfBounds)
{
    // We expect to have only tail in the tests below.

    // Older request checks.
    {
        // Check that all the data is newer than the current one.
        TestItems fetched = { kAboveItem };
        const auto result = makeData<Accessor>(kCurrentItems, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, 0);
        ASSERT_EQ(result.ranges.tail.length, 1);
    }

    {
        // Check that all the data is older than the current one.
        TestItems fetched = { kBelowItem };
        const auto result = makeData<Accessor>(kCurrentItems, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, 0);
        ASSERT_EQ(result.ranges.tail.length, 1);
    }

    // Newer request check.
    {
        // Check that all the data is older than the current one.
        TestItems fetched = { kBelowItem };
        const auto result = makeData<Accessor>(kCurrentItems, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, 0);
        ASSERT_EQ(result.ranges.tail.length, 1);
    }

    {
        // Check that all the data is newer than the current one.
        TestItems fetched = { kAboveItem };
        const auto result = makeData<Accessor>(kCurrentItems, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, 0);
        ASSERT_EQ(result.ranges.tail.length, 1);
    }
}

/**
 * We expect to have only shortened body if we fetched a copy of current data without central
 * point.
 */
TEST(FetchedData, sameFetchedDataWithRemovedPoint)
{
    // Older request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms         {0} 100ms             <- body
         * {1} 50ms
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{kCurrentItems.front()};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, 1);
        ASSERT_EQ(result.data[0].timestamp, milliseconds(100));

        ASSERT_EQ(result.ranges.tail.length, 0);
    }

    // Newer request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms
         * {1} 50ms          {1} 50ms              <- body
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{kCurrentItems.back()};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, 1);
        ASSERT_EQ(result.data[0].timestamp, milliseconds(50));

        ASSERT_EQ(result.ranges.tail.length, 0);
    }
}

/**
 * We treat any new item inside the current data (if separeted by other old items from the new
 * data)as a part of the body, so we expect to have only a body in all tests below.
 */
TEST(FetchedData, sameFetchedDataWithAddedItemInCurrentData)
{
    // Older request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         *                   {1} 100ms             <- new item, but included in body
         * {0} 50ms          {0} 50ms
         */
        const auto current = TestItems{kCurrentItems.back()};
        auto fetched = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, result.data.size());
        ASSERT_EQ(result.ranges.tail.length, 0);
    }

    // Newer request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms         {0} 100ms
         *                   {1} 50ms              <- new item, but included in body
         */
        const auto current = TestItems{kCurrentItems.front()};
        auto fetched = TestItems{kCurrentItems.back(), kCurrentItems.front()};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, result.data.size());
        ASSERT_EQ(result.ranges.tail.length, 0);
    }
}

/**
 * We expect to have tail consisting of this item if central point item is replaced.
 */
TEST(FetchedData, replaceOnlyCentralPointItem)
{
    // Older request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms         {0} 100ms             <- body
         * {1} 50ms          {2} 50ms              <- tail
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{kCurrentItems.front(),
            Item::create(kCurrentItems.back().timestamp)};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, 1);
        ASSERT_EQ(result.data[result.ranges.body.offset].timestamp, milliseconds(100));

        ASSERT_EQ(result.ranges.tail.length, 1);
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(50));
    }

    // Newer request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms         {2} 100ms             <- tail
         * {1} 50ms          {1} 50ms              <- body
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{kCurrentItems.back(),
            Item::create(kCurrentItems.front().timestamp)};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, 1);
        ASSERT_EQ(result.data[result.ranges.body.offset].timestamp, milliseconds(50));

        ASSERT_EQ(result.ranges.tail.length, 1);
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(100));
    }
}

/**
 * We expect only tail if we can't find central point at all.
 */
TEST(FetchedData, noCentralPointAtAll)
{
    // Older request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         *                   {1} 100ms             <- all tail
         * {0} 50ms          {2} 50ms
         */
        const auto current = TestItems{kCurrentItems.back()};
        auto fetched = TestItems{kCurrentItems.front(),
            Item::create(kCurrentItems.back().timestamp)};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, 0);

        ASSERT_EQ(result.ranges.tail.length, fetched.size());
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(100));
        ASSERT_EQ(result.data[result.ranges.tail.offset + 1].timestamp, milliseconds(50));
    }

    // Newer reuqest check
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms         {1} 100ms
         *                   {2} 50ms              <- all tail
         */
        const auto current = TestItems{kCurrentItems.front()};
        auto fetched = TestItems{kCurrentItems.back(),
            Item::create(kCurrentItems.front().timestamp)};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, 0);

        ASSERT_EQ(result.ranges.tail.length, fetched.size());
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(100));
        ASSERT_EQ(result.data[result.ranges.tail.offset + 1].timestamp, milliseconds(50));
    }
}

/**
 * Central point search when fetched in the bounds of current data. Expecting only tail.
 */
TEST(FetchedData, allFetchedInBoundsOfCurrentData)
{
    // Older request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms
         *                   {2} 90ms              <- tail
         * {1} 50ms
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{*(kCurrentItems.begin() + 1)};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, 0);

        ASSERT_EQ(result.ranges.tail.length, 1);
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(90));
    }

    // Newer request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms
         *                   {2} 60ms              <- tail
         * {1} 50ms
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{*(kCurrentItems.rbegin() + 1)};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, 0);

        ASSERT_EQ(result.ranges.tail.length, 1);
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(60));
    }
}

/**
 * Expecting only tail when fetched data cosist of new items and split to the two parts which both
 * outside current data.
 */
TEST(FetchedData, fetchedDataOutOfBoundsFromBothSides)
{
    // Older request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         *                   {2} 100ms             <- all tail
         * {0} 90ms
         * {1} 60ms
         *                   {3} 50ms
         */
        const auto current = TestItems{*(kCurrentItems.begin() + 1), *(kCurrentItems.rbegin() + 1)};
        auto fetched = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, 0);

        ASSERT_EQ(result.ranges.tail.length, 2);
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(100));
        ASSERT_EQ(result.data[result.ranges.tail.offset + 1].timestamp, milliseconds(50));
    }

    // Older request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         *                   {2} 100ms             <- all tail
         * {0} 90ms
         * {1} 60ms
         *                   {3} 50ms
         */
        const auto current = TestItems{*(kCurrentItems.begin() + 1), *(kCurrentItems.rbegin() + 1)};
        auto fetched = TestItems{kCurrentItems.back(), kCurrentItems.front()};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, 0);

        ASSERT_EQ(result.ranges.tail.length, 2);
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(100));
        ASSERT_EQ(result.data[result.ranges.tail.offset + 1].timestamp, milliseconds(50));
    }
}

/**
 * Two-iterations central point search. Check if central point move with existing body works
 * correctly.
 */
TEST(FetchedData, twoIterationsCentralPointSearch)
{
    // Older request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms         {0} 100ms             <- body
         *                   {2} 60ms              <- tail
         * {1} 50ms
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{current.front(), *(kCurrentItems.rbegin() + 1)};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, 1);
        ASSERT_EQ(result.data[result.ranges.body.offset].timestamp, milliseconds(100));

        ASSERT_EQ(result.ranges.tail.length, 1);
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(60));
    }

    // Newer request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms
         *                   {2} 90ms              <- tail
         * {1} 50ms          {1} 50ms              <- body
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{kCurrentItems.back(), *(kCurrentItems.begin() + 1)};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, 1);
        ASSERT_EQ(result.data[result.ranges.body.offset].timestamp, milliseconds(50));

        ASSERT_EQ(result.ranges.tail.length, 1);
        ASSERT_EQ(result.data[result.ranges.tail.offset].timestamp, milliseconds(90));
    }
}

/**
 * Expecting to have the body only when inserting new item in the middle of the current one.
 */
TEST(FetchedData, TTT)
{
    // Older request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms         {0} 100ms             <- all body
         *                   {2} 60ms
         * {1} 50ms          {1} 50ms
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{kCurrentItems.front(),
            *(kCurrentItems.rbegin() + 1),
            kCurrentItems.back()};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::older);

        ASSERT_EQ(result.ranges.body.length, fetched.size());
    }

    // Newer request check.
    {
        /**
         * Current({id} ms)  Fetched({id} ms)      Expected
         * {0} 100ms         {0} 100ms             <- all body
         *                   {2} 60ms
         * {1} 50ms          {1} 50ms
         */
        const auto current = TestItems{kCurrentItems.front(), kCurrentItems.back()};
        auto fetched = TestItems{kCurrentItems.back(),
            *(kCurrentItems.rbegin() + 1),
            kCurrentItems.front()};
        const auto result = makeData<Accessor>(current, fetched, FetchDirection::newer);

        ASSERT_EQ(result.ranges.body.length, fetched.size());
    }
}
