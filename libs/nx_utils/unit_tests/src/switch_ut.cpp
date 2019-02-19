#include <nx/utils/switch.h>

#include <gtest/gtest.h>

#include <nx/utils/nx_utils_ini.h>

namespace nx {
namespace utils {
namespace test {

static void expectVoice(const QString& animal, const QString& expectedVoice)
{
    const auto voice = switch_(animal,
        "cat", []{ return "mew"; },
        "dog", []{ return "wof"; },
        default_, []{ return "unknown"; }
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
        "a", []{ return Type::a; },
        "b", []{ return Type::b; }
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

static void expectToString(Type value, const QString& expectedString = {})
{
    const auto string = NX_ENUM_SWITCH(value,
    {
        case Type::a: return "a";
        case Type::b: return "b";
    });

    ASSERT_EQ(expectedString, string);
}

TEST(EnumSwitch, EnumToString)
{
    expectToString(Type::a, "a");
    expectToString(Type::b, "b");
    ASSERT_DEATH(expectToString(static_cast<Type>(42)), "Unmatched switch value: 42");
}

} // namespace test
} // namespace utils
} // namesapce nx
