// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <thread>

#include <nx/utils/log/assert.h>
#include <nx/utils/nx_utils_ini.h>

namespace nx::utils::test {

TEST(NxAssertHeavyCondition, All3)
{
    nx::kit::IniConfig::Tweaks iniTweaks;
    iniTweaks.set(&nx::utils::ini().assertCrash, true);
    iniTweaks.set(&nx::utils::ini().assertHeavyCondition, true);

    EXPECT_DEATH(NX_ASSERT_HEAVY_CONDITION(false), "");
    EXPECT_DEATH(NX_ASSERT_HEAVY_CONDITION(false, "oops"), "");
    EXPECT_DEATH(NX_ASSERT_HEAVY_CONDITION(false, "oops %1 != %2", "a", 7), "");
}

TEST(NxAssert, All3)
{
    nx::kit::IniConfig::Tweaks iniTweaks;
    iniTweaks.set(&nx::utils::ini().assertCrash, true);

    EXPECT_DEATH(NX_ASSERT(false), "");
    EXPECT_DEATH(NX_ASSERT(false, "oops"), "");
    EXPECT_DEATH(NX_ASSERT(false, "oops %1 != %2", "a", 7), "");

    EXPECT_DEATH((enableQtMessageAsserts(), qFatal("Fatal")), "");
    EXPECT_DEATH((enableQtMessageAsserts(), qCritical("Critical")), "");
    EXPECT_DEATH((enableQtMessageAsserts(), NX_FMT("%1", 1, 2)), "");
}

TEST(NxCritical, All3)
{
    EXPECT_DEATH(NX_CRITICAL(false), "");
    EXPECT_DEATH(NX_CRITICAL(false, "oops"), "");
    EXPECT_DEATH(NX_CRITICAL(false, "oops %1 != %2", "a", 7), "");
}

TEST(Freeze, DISABLED_Hour) { std::this_thread::sleep_for(std::chrono::hours(1)); }
TEST(Freeze, DISABLED_Seconds) { std::this_thread::sleep_for(std::chrono::seconds(5)); }

} // namespace nx::utils::test
