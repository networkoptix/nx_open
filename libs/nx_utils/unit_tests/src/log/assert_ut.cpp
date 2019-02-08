#include <gtest/gtest.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/nx_utils_ini.h>

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

    EXPECT_DEATH((enableQtMessageAsserts(), qFatal("Fatal")), "");
    EXPECT_DEATH((enableQtMessageAsserts(), qCritical("Critical")), "");
    EXPECT_DEATH((enableQtMessageAsserts(), log::Message("%1").args(1, 2)), "");
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
