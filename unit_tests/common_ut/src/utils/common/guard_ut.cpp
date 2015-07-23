#include <gtest/gtest.h>
#include <utils/common/guard.h>

TEST(UtilsCommon, Guard)
{
    int a = 1;
    {
        Guard g([&](){ a = 2; });
    }
    EXPECT_EQ(a, 2);
    {
        Guard g([&](){ a = 3; });
        g.disarm();
    }
    EXPECT_EQ(a, 2);
    {
        Guard g([&](){ a = 4; });
        g.fire();

        EXPECT_EQ(a, 4);
        a = 0;
    }
    ASSERT_EQ(a, 0);
}
