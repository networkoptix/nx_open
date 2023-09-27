// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/thread/barrier_handler.h>

namespace nx {
namespace utils {
namespace test {

TEST( BarrierHandler, Normal )
{
    int x = 0;
    MoveOnlyFunc<void()> h1, h2;
    {
        BarrierHandler b([&]() { x = 5; });
        h1 = b.fork();
        h2 = b.fork();
    }
    ASSERT_EQ( x, 0 );
    h1();
    ASSERT_EQ( x, 0 );
    h2();
    ASSERT_EQ( x, 5 );
}

TEST( BarrierHandler, Fast )
{
    int x = 0;
    {
        BarrierHandler b( [ & ](){ x = 5; } );
        {
            auto h1 = b.fork();
            auto h2 = b.fork();
        }
        ASSERT_EQ( x, 0 );
    }
    ASSERT_EQ( x, 5 );
}

} // namespace test
} // namespace utils
} // namespace nx
