#include <gtest/gtest.h>

#include <nx/utils/log/log_level.h>

namespace nx {
namespace utils {
namespace log {
namespace test {

TEST(LogLevel, FromString)
{
    ASSERT_EQ(Level::undefined, levelFromString("wtf"));
    ASSERT_EQ(Level::undefined, levelFromString(""));
    ASSERT_EQ(Level::undefined, levelFromString("hello world"));

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

    ASSERT_EQ(Level::notConfigured, levelFromString("notConfigured"));
    ASSERT_EQ(Level::notConfigured, levelFromString("not_configured"));
    ASSERT_EQ(Level::notConfigured, levelFromString("NOT_CONFIGURED"));
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
    ASSERT_EQ("notConfigured", toString(Level::notConfigured));
}

void testParsing(const QString& s, Level p, std::map<const char*, Level> fs = {})
{
    LevelSettings settings;
    settings.parse(s);

    LevelFilters filters;
    for (const auto& f: fs)
        filters.emplace(Tag(QString::fromUtf8(f.first)), f.second);

    EXPECT_EQ(LevelSettings(p, filters), settings) << s.toStdString();
}

TEST(LogLevelSettings, Parse)
{
    testParsing("info", Level::info);

    testParsing("debug, verbose[nx::Class]", Level::debug, {{"nx::Class", Level::verbose}});

    testParsing("verbose[nx::Class1, nx::Class2]", kDefaultLevel,
        {{"nx::Class1", Level::verbose}, {"nx::Class2", Level::verbose}});

    testParsing("verbose[nx::Class1,nx::Class2],warning", Level::warning,
        {{"nx::Class1", Level::verbose}, {"nx::Class2", Level::verbose}});

    testParsing("i,d[aaa],v[bbb],i[ccc]", Level::info, {{"aaa", Level::debug},
        {"bbb", Level::verbose}, {"ccc", Level::info}});

    testParsing("info, info[ space::Class ], debug[ Class(0x123) ]", Level::info,
        {{"space::Class", Level::info}, {"Class(0x123)", Level::debug}});

    testParsing("info[x,y], verbose[a,b], none", Level::none,
        {{"x", Level::info}, {"y", Level::info}, {"a", Level::verbose}, {"b", Level::verbose}});

    testParsing("verbose[First], WrongLevel[Second, Third], none[Foth]", kDefaultLevel,
        {{"First", Level::verbose}, {"Foth", Level::none}});

    testParsing("info, verbose[First, NoBracket", Level::info,
        {{"First", Level::verbose}, {"NoBracket", Level::verbose}});

    testParsing("info, verbose[First, NoBracket], ] wrongCloser, none[OneMore]", Level::info,
        {{"First", Level::verbose}, {"NoBracket", Level::verbose}, {"OneMore", Level::none}});

    testParsing("info, verbose[First [NestedBracket] Not], debug[X]", Level::info,
        {{"First", Level::verbose}, {"NestedBracket", Level::verbose}, {"X", Level::debug}});
}

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
