#include <nx/utils/switch.h>

#include <gtest/gtest.h>

#include <nx/utils/nx_utils_ini.h>

namespace nx::utils::test {

static void expectVoice(const QString& animal, const QString& expectedVoice)
{
    const auto voice = switch_(animal,
        "cat", [](){ return "mew"; },
        "dog", [](){ return "wof"; },
        default_, [](){ return "unknown"; }
    );

    ASSERT_EQ(expectedVoice, voice);
}

TEST(Switch, StringToString)
{
    expectVoice("cat", "mew");
    expectVoice("dog", "wof");
    expectVoice("parrot", "unknown");
    expectVoice("mouse", "unknown");
}

enum class Type { a, b };

static void expectParsedType(const QString& value, Type expectedType = Type::a)
{
    const auto parsedType = switch_(value,
        "a", [](){ return Type::a; },
        "b", [](){ return Type::b; }
    );

    ASSERT_EQ(expectedType, parsedType);
}

TEST(Switch, StringToEnum)
{
    expectParsedType("a", Type::a);
    expectParsedType("b", Type::b);
    if (ini().assertCrash)
    {
        ASSERT_DEATH(expectParsedType("c"), "Unmatched switch value");
        ASSERT_DEATH(expectParsedType("d"), "Unmatched switch value");
    }
}

TEST(Switch, Action)
{
    int value = 0;
    const auto execute =
        [&value](const QString& command)
        {
            switch_(command,
                "increment", [&](){ value++; },
                "null", [&](){ value = 0; }
            );
        };

    execute("increment");
    ASSERT_EQ(1, value);

    execute("increment");
    ASSERT_EQ(2, value);

    execute("null");
    ASSERT_EQ(0, value);

    if (ini().assertCrash)
    {
        ASSERT_DEATH(execute("wrong"), "Unmatched switch value");
        ASSERT_DEATH(execute("command"), "Unmatched switch value");
    }
}

} // namespace nx::utils::test
