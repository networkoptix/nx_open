// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/data_structures/keyed_list.h>
#include <nx/utils/uuid.h>

namespace nx::utils::test {

TEST(KeyedListTest, basics)
{
    KeyedList<int, QString> list;
    ASSERT_TRUE(list.empty());

    // Pushing elements:
    ASSERT_TRUE(list.push_back(1, "value 1"));
    ASSERT_TRUE(list.push_back(10, "value 10"));
    ASSERT_FALSE(list.push_back(10, "value 10 again")); //< Duplucate push back.
    ASSERT_FALSE(list.push_front(1, "value 1 again")); //< Duplucate push front.

    ASSERT_TRUE(list.push_back(0, "value 0"));
    ASSERT_TRUE(list.push_back(5, "value 5"));
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

TEST(KeyedListTest, pushBack)
{
    KeyedList<int, QString> list;
    list.push_back(0, "value 0");
    ASSERT_EQ(list.size(), 1);
    ASSERT_EQ(list.key(0), 0);
    ASSERT_EQ(list.value(0), "value 0");

    list.push_back(1, "value 1");
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.key(0), 0);
    ASSERT_EQ(list.value(0), "value 0");
    ASSERT_EQ(list.key(1), 1);
    ASSERT_EQ(list.value(1), "value 1");

    list.push_back(0, "another value 0");
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.key(0), 0);
    ASSERT_EQ(list.value(0), "value 0");
    ASSERT_EQ(list.key(1), 1);
    ASSERT_EQ(list.value(1), "value 1");

    list.push_back(-1, "value negative 1");
    ASSERT_EQ(list.size(), 3);
    ASSERT_EQ(list.key(0), 0);
    ASSERT_EQ(list.value(0), "value 0");
    ASSERT_EQ(list.key(1), 1);
    ASSERT_EQ(list.value(1), "value 1");
    ASSERT_EQ(list.key(2), -1);
    ASSERT_EQ(list.value(-1), "value negative 1");
}

TEST(KeyedListTest, pushFront)
{
    KeyedList<int, QString> list;
    list.push_front(0, "value 0");
    ASSERT_EQ(list.size(), 1);
    ASSERT_EQ(list.key(0), 0);
    ASSERT_EQ(list.value(0), "value 0");

    list.push_front(1, "value 1");
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.key(0), 1);
    ASSERT_EQ(list.value(0), "value 0");
    ASSERT_EQ(list.key(1), 0);
    ASSERT_EQ(list.value(1), "value 1");

    list.push_front(0, "another value 0");
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.key(0), 1);
    ASSERT_EQ(list.value(0), "value 0");
    ASSERT_EQ(list.key(1), 0);
    ASSERT_EQ(list.value(1), "value 1");

    list.push_front(-1, "value negative 1");
    ASSERT_EQ(list.size(), 3);
    ASSERT_EQ(list.key(0), -1);
    ASSERT_EQ(list.value(0), "value 0");
    ASSERT_EQ(list.key(1), 1);
    ASSERT_EQ(list.value(1), "value 1");
    ASSERT_EQ(list.key(2), 0);
    ASSERT_EQ(list.value(-1), "value negative 1");
}

TEST(KeyedListTest, copyConstructor)
{
    KeyedList<int, QString> sourceList;
    sourceList.push_back(1, "value 1");
    sourceList.push_back(5, "value 5");
    sourceList.push_back(0, "value 0");
    sourceList.push_back(3, "value 3");

    KeyedList<int, QString> list(sourceList);
    ASSERT_FALSE(list.empty());
    ASSERT_EQ(list.size(), 4);

    ASSERT_EQ(list.key(0), 1);
    ASSERT_EQ(list.key(1), 5);
    ASSERT_EQ(list.key(2), 0);
    ASSERT_EQ(list.key(3), 3);

    ASSERT_EQ(list[0], "value 1");
    ASSERT_EQ(list[1], "value 5");
    ASSERT_EQ(list[2], "value 0");
    ASSERT_EQ(list[3], "value 3");

    ASSERT_EQ(list.value(0), "value 0");
    ASSERT_EQ(list.value(1), "value 1");
    ASSERT_EQ(list.value(2), "");
    ASSERT_EQ(list.value(3), "value 3");
    ASSERT_EQ(list.value(4), "");
    ASSERT_EQ(list.value(5), "value 5");
}

TEST(KeyedListTest, initializationList)
{
}

TEST(KeyedListTest, remove)
{
    KeyedList<int, QString> list;
    list.push_back(1, "value 1");
    list.push_back(2, "value 2");
    list.push_back(3, "value 3");

    list.push_front(4, "value 4");
    list.push_front(5, "value 5");
    list.push_front(6, "value 6");

    ASSERT_EQ(list.size(), 6);
    list.remove(3);
    list.remove(4);
    list.remove(4); //< Deleting the same key again.
    list.remove(10); //< Trying to remove nonexistent key.
    ASSERT_EQ(list.size(), 4);

    ASSERT_EQ(list[0], "value 6");
    ASSERT_EQ(list[1], "value 5");
    ASSERT_EQ(list[2], "value 1");
    ASSERT_EQ(list[3], "value 2");
}

TEST(KeyedListTest, removeAt)
{
    KeyedList<int, QString> list;
    list.push_back(1, "value 1");
    list.push_back(2, "value 2");
    list.push_back(3, "value 3");

    list.push_front(4, "value 4");
    list.push_front(5, "value 5");
    list.push_front(6, "value 6");

    list.removeAt(3);
    list.removeAt(4);
    ASSERT_EQ(list.size(), 4);

    ASSERT_EQ(list[0], "value 6");
    ASSERT_EQ(list[1], "value 5");
    ASSERT_EQ(list[2], "value 4");
    ASSERT_EQ(list[3], "value 2");
}

TEST(KeyedListTest, indexOf)
{
    KeyedList<int, QString> list;
    list.push_back(0, "value 0");
    list.push_back(1, "value 1");
    list.push_back(2, "value 2");
    list.push_back(3, "value 3");
    list.push_back(4, "value 4");
    list.push_back(5, "value 5");

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

TEST(KeyedListTest, value)
{
    KeyedList<int, QString> list;
    list.push_back(0, "value 0");
    list.push_back(1, "value 1");
    list.push_back(2, "value 2");
    list.push_back(3, "value 3");
    list.push_back(4, "value 4");
    list.push_back(5, "value 5");

    ASSERT_EQ(list.value(0), "value 0");
    ASSERT_EQ(list.value(1), "value 1");
    ASSERT_EQ(list.value(2), "value 2");
    ASSERT_EQ(list.value(3), "value 3");
    ASSERT_EQ(list.value(4), "value 4");
    ASSERT_EQ(list.value(5), "value 5");
    ASSERT_EQ(list.value(6, "Not exists"), "Not exists");
}

} // namespace nx::utils::test
