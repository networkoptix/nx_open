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

TEST(Time, parseDuration)
{
    using namespace std::chrono;
    {
        const auto result = parseDuration("2h");
        ASSERT_EQ(result.first, true);
        ASSERT_EQ(result.second, hours(2));
    }
    {
        const auto result = parseDuration("2H");
        ASSERT_EQ(result.first, true);
        ASSERT_EQ(result.second, hours(2));
    }
    {
        const auto result = parseDuration("1d");
        ASSERT_EQ(result.first, true);
        ASSERT_EQ(result.second, hours(24));
    }
    {
        const auto result = parseDuration("256m");
        ASSERT_EQ(result.first, true);
        ASSERT_EQ(result.second, minutes(256));
    }
    {
        const auto result = parseDuration("2s");
        ASSERT_EQ(result.first, true);
        ASSERT_EQ(result.second, seconds(2));
    }
    {
        const auto result = parseDuration("2");
        ASSERT_EQ(result.first, true);
        ASSERT_EQ(result.second, milliseconds(2000));
    }
    {
        const auto result = parseDuration("2ms");
        ASSERT_EQ(result.first, true);
        ASSERT_EQ(result.second, milliseconds(2));
    }
    {
        const auto result = parseDuration("1234567890ms");
        ASSERT_EQ(result.first, true);
        ASSERT_EQ(result.second, milliseconds(1234567890));
    }

    {
        const auto result = parseDuration("qqms");
        ASSERT_EQ(result.first, false);
    }
    {
        const auto result = parseDuration("qqms123");
        ASSERT_EQ(result.first, false);
    }
    {
        const auto result = parseDuration("h1");
        ASSERT_EQ(result.first, false);
    }
    {
        const auto result = parseDuration("1hs");
        ASSERT_EQ(result.first, false);
    }
    {
        const auto result = parseDuration("h");
        ASSERT_EQ(result.first, false);
    }
    {
        const auto result = parseDuration("ms");
        ASSERT_EQ(result.first, false);
    }
}


} // namespace nx::utils
