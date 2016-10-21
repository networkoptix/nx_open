#include <gtest/gtest.h>
#include <nx/utils/log/assert.h>

// Current version of google test doesn't support it for WIN32
#ifndef Q_OS_WIN32

#ifdef _DEBUG
TEST(NxExpect, All3)
#else
TEST(NxExpect, DISABLED_All3)
#endif
{
    EXPECT_DEATH(NX_EXPECT(false), "");
    EXPECT_DEATH(NX_EXPECT(false, "oops"), "");
    EXPECT_DEATH(NX_EXPECT(false, "here", "oops"), "");
}

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

#endif // Q_OS_WIN32