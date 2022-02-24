// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <utils/common/threadqueue.h>

TEST(ThreadQueue, pack)
{
    QnSafeQueue<int> queue;
    for (int i = 0; i <= 10; ++i)
        queue.push(i);

    int value = 0;
    queue.pop(value);
    queue.push(11);

    auto access = queue.lock();


    access.setAt(int(), /*index*/ 1);
    access.setAt(int(), /*index*/ 5);
    access.setAt(int(), /*index*/ 8);
    access.pack();

    ASSERT_EQ(8, access.size());

    ASSERT_EQ(1, access.front());
    access.popFront();
    ASSERT_EQ(3, access.front());
    access.popFront();
    ASSERT_EQ(4, access.front());
    access.popFront();
    ASSERT_EQ(5, access.front());
    access.popFront();
    ASSERT_EQ(7, access.front());
    access.popFront();
    ASSERT_EQ(8, access.front());
    access.popFront();
    ASSERT_EQ(10, access.front());
    access.popFront();
    ASSERT_EQ(11, access.front());
    access.popFront();
}

TEST(ThreadQueue, pack2)
{
    QnSafeQueue<int> queue;
    for (int i = 1; i <= 10; ++i)
        queue.push(i);

    auto access = queue.lock();
    access.pack();

    ASSERT_EQ(10, access.size());
    for (int i = 1; i <= 10; ++i)
    {
        ASSERT_EQ(i, access.front());
        access.popFront();
    }
}
