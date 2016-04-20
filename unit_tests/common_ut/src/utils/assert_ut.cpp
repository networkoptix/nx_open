#include <gtest/gtest.h>
#include <nx/utils/log/assert.h>

#ifdef _DEBUG
TEST(NxAssert, All3)
#else
TEST(NxAssert, DISABLED_All3)
#endif
{
    EXPECT_DEATH(NX_ASSERT(false), "");
    EXPECT_DEATH(NX_ASSERT(false, "oops"), "");
    EXPECT_DEATH(NX_ASSERT(false, "here", "oops"), "");
}

TEST(NxCritical, All3)
{
    EXPECT_DEATH(NX_CRITICAL(false), "");
    EXPECT_DEATH(NX_CRITICAL(false, "oops"), "");
    EXPECT_DEATH(NX_CRITICAL(false, "here", "oops"), "");
}
