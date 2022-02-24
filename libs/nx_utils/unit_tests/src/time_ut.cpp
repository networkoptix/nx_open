// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <cstdlib>

#include <gtest/gtest.h>

#include <nx/utils/time.h>

namespace nx::utils {

TEST(Time, timeSinceEpoch_equals_time_t)
{
    using namespace std::chrono;

    const auto testStart = steady_clock::now();

    const auto t1 = nx::utils::timeSinceEpoch();
    const auto t2 = ::time(NULL);

    const auto testRunTime = steady_clock::now() - testStart;

    ASSERT_LE(
        std::abs(t2 - duration_cast<seconds>(t1).count()),
        duration_cast<seconds>(testRunTime).count()+1);
}

TEST(Time, parseDurationCorrect)
{
    using namespace std::chrono;
    {
        const auto result = parseDuration("2h");
        ASSERT_TRUE(result);
        ASSERT_EQ(*result, 2h);
    }
    {
        const auto result = parseDuration("2H");
        ASSERT_TRUE(result);
        ASSERT_EQ(*result, 2h);
    }
    {
        const auto result = parseDuration("1d");
        ASSERT_TRUE(result);
        ASSERT_EQ(*result, 24h);
    }
    {
        const auto result = parseDuration("256m");
        ASSERT_TRUE(result);
        ASSERT_EQ(*result, 256min);
    }
    {
        const auto result = parseDuration("2s");
        ASSERT_TRUE(result);
        ASSERT_EQ(*result, 2s);
    }
    {
        const auto result = parseDuration("2");
        ASSERT_TRUE(result);
        ASSERT_EQ(*result, 2000ms);
    }
    {
        const auto result = parseDuration("2ms");
        ASSERT_TRUE(result);
        ASSERT_EQ(*result, 2ms);
    }
    {
        const auto result = parseDuration("1234567890ms");
        ASSERT_TRUE(result);
        ASSERT_EQ(*result, milliseconds(1234567890));
    }
}

TEST(Time, parseDurationIncorrect)
{
    {
        const auto result = parseDuration("qqms");
        ASSERT_FALSE(result);
    }
    {
        const auto result = parseDuration("qqms123");
        ASSERT_FALSE(result);
    }
    {
        const auto result = parseDuration("h1");
        ASSERT_FALSE(result);
    }
    {
        const auto result = parseDuration("1hs");
        ASSERT_FALSE(result);
    }
    {
        const auto result = parseDuration("h");
        ASSERT_FALSE(result);
    }
    {
        const auto result = parseDuration("ms");
        ASSERT_FALSE(result);
    }
}

TEST(Time, durationToString)
{
    ASSERT_EQ("1h", durationToString(std::chrono::seconds(3600)));
    ASSERT_EQ("1ms", durationToString(std::chrono::milliseconds(1)));
    ASSERT_EQ("1m", durationToString(std::chrono::milliseconds(60000)));
}

} // namespace nx::utils
