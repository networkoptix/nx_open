#include <gtest/gtest.h>

#include <nx/utils/log/log_level.h>

namespace nx {
namespace utils {
namespace log {
namespace test {

TEST(LogLevel, FromString)
{
    ASSERT_EQ(Level::none, levelFromString("none"));
    ASSERT_EQ(Level::none, levelFromString("NONE"));
    ASSERT_EQ(Level::none, levelFromString("n"));

    ASSERT_EQ(Level::always, levelFromString("always"));
    ASSERT_EQ(Level::always, levelFromString("ALWAYS"));
    ASSERT_EQ(Level::always, levelFromString("a"));

    ASSERT_EQ(Level::error, levelFromString("error"));
    ASSERT_EQ(Level::error, levelFromString("ERROR"));
    ASSERT_EQ(Level::error, levelFromString("e"));

    ASSERT_EQ(Level::warning, levelFromString("warning"));
    ASSERT_EQ(Level::warning, levelFromString("WARNING"));
    ASSERT_EQ(Level::warning, levelFromString("w"));

    ASSERT_EQ(Level::info, levelFromString("info"));
    ASSERT_EQ(Level::info, levelFromString("INFO"));
    ASSERT_EQ(Level::info, levelFromString("i"));

    ASSERT_EQ(Level::debug, levelFromString("debug"));
    ASSERT_EQ(Level::debug, levelFromString("DEBUG"));
    ASSERT_EQ(Level::debug, levelFromString("DEBUG1"));
    ASSERT_EQ(Level::debug, levelFromString("d"));

    ASSERT_EQ(Level::verbose, levelFromString("verbose"));
    ASSERT_EQ(Level::verbose, levelFromString("VERBOSE"));
    ASSERT_EQ(Level::verbose, levelFromString("v"));

    ASSERT_EQ(Level::verbose, levelFromString("debug2"));
    ASSERT_EQ(Level::verbose, levelFromString("DEBUG2"));
    ASSERT_EQ(Level::verbose, levelFromString("v"));

    ASSERT_EQ(Level::undefined, levelFromString("wtf"));
    ASSERT_EQ(Level::undefined, levelFromString(""));
    ASSERT_EQ(Level::undefined, levelFromString("hello world"));
}

TEST(LogLevel, ToString)
{
    ASSERT_EQ("undefined", toString(Level::undefined));
    ASSERT_EQ("none", toString(Level::none));
    ASSERT_EQ("always", toString(Level::always));
    ASSERT_EQ("error", toString(Level::error));
    ASSERT_EQ("warning", toString(Level::warning));
    ASSERT_EQ("info", toString(Level::info));
    ASSERT_EQ("debug", toString(Level::debug));
    ASSERT_EQ("verbose", toString(Level::verbose));
}

TEST(LogLevelFilters, FromString)
{
    ASSERT_EQ(
        (LevelFilters{{"Single", Level::verbose}}),
        levelFiltersFromString("Single"));

    ASSERT_EQ(
        (LevelFilters{{"aaa", Level::debug}, {"bbb", Level::verbose}, {"ccc", Level::info}}),
        levelFiltersFromString("aaa-debug;bbb;ccc-info"));

    ASSERT_EQ(
        (LevelFilters{{"space::Class", Level::info}, {"Class(0x123)", Level::debug}}),
        levelFiltersFromString("space::Class-i;Class(0x123)-d"));

    ASSERT_EQ(
        (LevelFilters{
            {"x", Level::info}, {"y", Level::info}, {"a", Level::verbose}, {"b", Level::verbose}}),
        levelFiltersFromString("x,y-info;a,b"));

    ASSERT_EQ(
        (LevelFilters{{"First", Level::verbose}}),
        levelFiltersFromString("First;Second-WrongLevel"));

    ASSERT_EQ(
        (LevelFilters{{"Second", Level::verbose}}),
        levelFiltersFromString("Second;wrong-filter-format"));
}

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
