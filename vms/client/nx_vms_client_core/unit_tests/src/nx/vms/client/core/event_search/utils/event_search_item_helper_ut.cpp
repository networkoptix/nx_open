// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <deque>

#include <QtCore/QString>

#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>

#include "structures.h"

namespace nx::vms::client::core::event_search {
namespace test {

using FD = EventSearch::FetchDirection;

TEST(removeDuplicateItems, emptyContainer)
{
    std::deque<Item> data;

    EXPECT_EQ(removeDuplicateItems<Accessor>(data, data.begin(), data.end(), FD::newer), 0);
    EXPECT_TRUE(data.empty());

    EXPECT_EQ(removeDuplicateItems<Accessor>(data, data.begin(), data.end(), FD::older), 0);
    EXPECT_TRUE(data.empty());
}

TEST(removeDuplicateItems, noDuplicates_newer)
{
    std::deque<Item> data{
        Item::create("a", 1ms),
        Item::create("b", 1ms),
        Item::create("c", 2ms)};

    EXPECT_EQ(removeDuplicateItems<Accessor>(data, data.begin(), data.end(), FD::newer), 0);

    EXPECT_EQ(data,
        std::deque<Item>({
            Item::create("a", 1ms),
            Item::create("b", 1ms),
            Item::create("c", 2ms)}));
}

TEST(removeDuplicateItems, noDuplicates_older)
{
    std::deque<Item> data{
        Item::create("a", 2ms),
        Item::create("b", 2ms),
        Item::create("c", 1ms)};

    EXPECT_EQ(removeDuplicateItems<Accessor>(data, data.begin(), data.end(), FD::older), 0);

    EXPECT_EQ(data,
        std::deque<Item>({
            Item::create("a", 2ms),
            Item::create("b", 2ms),
            Item::create("c", 1ms)}));
}

TEST(removeDuplicateItems, exactDuplicates_newer)
{
    std::deque<Item> data{
        Item::create("a", 1ms),
        Item::create("a", 1ms),
        Item::create("b", 1ms),
        Item::create("c", 2ms),
        Item::create("c", 2ms)};

    EXPECT_EQ(removeDuplicateItems<Accessor>(data, data.begin(), data.end(), FD::newer), 2);

    EXPECT_EQ(data,
        std::deque<Item>({
            Item::create("a", 1ms),
            Item::create("b", 1ms),
            Item::create("c", 2ms)}));
}

TEST(removeDuplicateItems, exactDuplicates_older)
{
    std::deque<Item> data{
        Item::create("a", 2ms),
        Item::create("a", 2ms),
        Item::create("b", 2ms),
        Item::create("c", 1ms),
        Item::create("c", 1ms)};

    EXPECT_EQ(removeDuplicateItems<Accessor>(data, data.begin(), data.end(), FD::older), 2);

    EXPECT_EQ(data,
        std::deque<Item>({
            Item::create("a", 2ms),
            Item::create("b", 2ms),
            Item::create("c", 1ms)}));
}

TEST(removeDuplicateItems, inexactDuplicates_newer)
{
    std::deque<Item> data{
        Item::create("a", 1ms),
        Item::create("b", 1ms),
        Item::create("a", 2ms),
        Item::create("c", 2ms),
        Item::create("a", 3ms),
        Item::create("d", 4ms),
        Item::create("c", 5ms)};

    EXPECT_EQ(removeDuplicateItems<Accessor>(data, data.begin(), data.end(), FD::newer), 3);

    // Duplicates with the oldest time are kept.
    EXPECT_EQ(data,
        std::deque<Item>({
            Item::create("a", 1ms),
            Item::create("b", 1ms),
            Item::create("c", 2ms),
            Item::create("d", 4ms)}));
}

TEST(removeDuplicateItems, inexactDuplicates_older)
{
    std::deque<Item> data{
        Item::create("a", 5ms),
        Item::create("b", 4ms),
        Item::create("a", 3ms),
        Item::create("c", 3ms),
        Item::create("a", 2ms),
        Item::create("d", 2ms),
        Item::create("c", 1ms)};

    EXPECT_EQ(removeDuplicateItems<Accessor>(data, data.begin(), data.end(), FD::older), 3);

    // Duplicates with the oldest time are kept.
    EXPECT_EQ(data,
        std::deque<Item>({
            Item::create("b", 4ms),
            Item::create("a", 2ms),
            Item::create("d", 2ms),
            Item::create("c", 1ms)}));
}

} // namespace test
} // namespace nx::vms::client::core::event_search
