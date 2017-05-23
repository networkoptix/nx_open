#include <gtest/gtest.h>
#include <nx/utils/scope_guard.h>

TEST(UtilsCommon, Guard)
{
    int a = 1;
    {
        Guard g([&](){ a = 2; });
        ASSERT_TRUE( static_cast<bool>(g) );
    }
    ASSERT_EQ(a, 2);
    {
        Guard g([&](){ a = 3; });
        g.disarm();
        ASSERT_FALSE( g );
    }
    ASSERT_EQ(a, 2);
    {
        Guard g([&](){ a = 4; });
        g.fire();

        ASSERT_EQ(a, 4);
        a = 0;
    }
    ASSERT_EQ(a, 0);
    {
        Guard g1([&](){ a = -1; });
        Guard g2([&](){ a = 1; });
        Guard g3([&](){ a = 2; });

        g1.disarm();
    }
    ASSERT_EQ(a, 1);
    {
        Guard g1([&](){ a = 2; });
        Guard g2([&](){ a = 3; });

        g1 = std::move( g2 );
        ASSERT_EQ(a, 2);
    }
    ASSERT_EQ(a, 3);
}
