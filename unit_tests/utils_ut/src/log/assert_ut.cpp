#include <gtest/gtest.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace utils {
namespace test {

// Current version of google test doesn't support crash tests on WIN32.
#ifndef Q_OS_WIN32

TEST(NxAssertHeavyCondition, All3)
{
    if (!ini().assertHeavyCondition || !ini().assertCrash)
        return;

    EXPECT_DEATH(NX_ASSERT_HEAVY_CONDITION(false), "");
    EXPECT_DEATH(NX_ASSERT_HEAVY_CONDITION(false, "oops"), "");
    EXPECT_DEATH(NX_ASSERT_HEAVY_CONDITION(false, "here", "oops"), "");
}

TEST(NxAssert, All3)
{
    if (!ini().assertCrash)
        return;

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

#endif // Q_OS_WIN32

} // namespace test
} // namespace utils
} // namespace nx
