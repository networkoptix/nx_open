// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/data_structures/keyed_list.h>
#include <nx/utils/uuid.h>

namespace nx::utils::test {

TEST(KeyListTest, basics)
{
    KeyList<int> list;
    ASSERT_TRUE(list.empty());

    // Pushing elements:
    ASSERT_TRUE(list.push_back(1));
    ASSERT_TRUE(list.push_back(10));
    ASSERT_FALSE(list.push_back(10)); //< Duplicate push back.
    ASSERT_FALSE(list.push_front(1)); //< Duplucate push front.

    ASSERT_TRUE(list.push_back(0));
    ASSERT_TRUE(list.push_front(5));

    ASSERT_EQ(list.size(), 4);

    ASSERT_TRUE(list.contains(0));
    ASSERT_TRUE(list.contains(1));
    ASSERT_FALSE(list.contains(2));
    ASSERT_TRUE(list.contains(5));
    ASSERT_TRUE(list.contains(10));
    ASSERT_FALSE(list.contains(-1));

    list.clear();
    ASSERT_TRUE(list.empty());
    ASSERT_EQ(list.size(), 0);
}

TEST(KeyListTest, pushBack)
{
    KeyList<int> list;
    list.push_back(0);
    ASSERT_EQ(list.size(), 1);
    ASSERT_EQ(list.key(0), 0);

    list.push_back(1);
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.key(0), 0);
    ASSERT_EQ(list.key(1), 1);

    list.push_back(0);
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.key(0), 0);
    ASSERT_EQ(list.key(1), 1);

    list.push_back(-1);
    ASSERT_EQ(list.size(), 3);
    ASSERT_EQ(list.key(0), 0);
    ASSERT_EQ(list.key(1), 1);
    ASSERT_EQ(list.key(2), -1);
}

TEST(KeyListTest, pushFront)
{
    KeyList<int> list;
    list.push_front(0);
    ASSERT_EQ(list.size(), 1);
    ASSERT_EQ(list.key(0), 0);

    list.push_front(1);
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.key(0), 1);
    ASSERT_EQ(list.key(1), 0);

    list.push_front(0);
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.key(0), 1);
    ASSERT_EQ(list.key(1), 0);

    list.push_front(-1);
    ASSERT_EQ(list.size(), 3);
    ASSERT_EQ(list.key(0), -1);
    ASSERT_EQ(list.key(1), 1);
    ASSERT_EQ(list.key(2), 0);
}

TEST(KeyListTest, copyConstructor)
{
    KeyList<int> sourceList;
    sourceList.push_back(1);
    sourceList.push_back(5);
    sourceList.push_back(0);
    sourceList.push_back(3);

    KeyList<int> list(sourceList);
    ASSERT_FALSE(list.empty());
    ASSERT_EQ(list.size(), 4);
    ASSERT_EQ(list[0], 1);
    ASSERT_EQ(list[1], 5);
    ASSERT_EQ(list[2], 0);
    ASSERT_EQ(list[3], 3);

    ASSERT_EQ(list.key(0), 1);
    ASSERT_EQ(list.key(1), 5);
    ASSERT_EQ(list.key(2), 0);
    ASSERT_EQ(list.key(3), 3);
}

TEST(KeyListTest, initializationList)
{
    QList<int> sourceList = {1, 5, 0, 3, 0};
    KeyList<int> list(sourceList);
    ASSERT_FALSE(list.empty());
    ASSERT_EQ(list.size(), 4);
    ASSERT_EQ(list[0], 1);
    ASSERT_EQ(list[1], 5);
    ASSERT_EQ(list[2], 0);
    ASSERT_EQ(list[3], 3);

    ASSERT_EQ(list.key(0), 1);
    ASSERT_EQ(list.key(1), 5);
    ASSERT_EQ(list.key(2), 0);
    ASSERT_EQ(list.key(3), 3);
}

TEST(KeyListTest, remove)
{
    KeyList<int> list;
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);

    list.push_front(4);
    list.push_front(5);
    list.push_front(6);

    ASSERT_EQ(list.size(), 6);
    list.remove(3);
    list.remove(4);
    list.remove(4); //< Deleting the same key again.
    list.remove(10); //< Trying to remove nonexistent key.
    ASSERT_EQ(list.size(), 4);

    ASSERT_EQ(list[0], 6);
    ASSERT_EQ(list[1], 5);
    ASSERT_EQ(list[2], 1);
    ASSERT_EQ(list[3], 2);
}

TEST(KeyListTest, removeAt)
{
    KeyList<int> list;
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);

    list.push_front(4);
    list.push_front(5);
    list.push_front(6);

    list.removeAt(3);
    list.removeAt(4);
    ASSERT_EQ(list.size(), 4);

    ASSERT_EQ(list[0], 6);
    ASSERT_EQ(list[1], 5);
    ASSERT_EQ(list[2], 4);
    ASSERT_EQ(list[3], 2);
}

TEST(KeyListTest, indexOf)
{
    KeyList<int> list;
    list.push_back(0);
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);
    list.push_back(4);
    list.push_back(5);

    ASSERT_EQ(list.index_of(0), 0);
    ASSERT_EQ(list.index_of(1), 1);
    ASSERT_EQ(list.index_of(2), 2);
    ASSERT_EQ(list.index_of(3), 3);
    ASSERT_EQ(list.index_of(4), 4);
    ASSERT_EQ(list.index_of(5), 5);

    list.remove(0);
    list.remove(4);

    ASSERT_EQ(list.index_of(0), -1);
    ASSERT_EQ(list.index_of(1), 0);
    ASSERT_EQ(list.index_of(2), 1);
    ASSERT_EQ(list.index_of(3), 2);
    ASSERT_EQ(list.index_of(4), -1);
    ASSERT_EQ(list.index_of(5), 3);
}

} // namespace nx::utils::test
