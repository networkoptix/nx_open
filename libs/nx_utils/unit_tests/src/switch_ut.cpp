#include <nx/utils/switch.h>

#include <gtest/gtest.h>

#include <nx/utils/nx_utils_ini.h>

namespace nx {
namespace utils {
namespace test {

static QString voice(const QString& animal)
{
    return switch_(animal,
        "cat", []{ return "mew"; },
        "dog", []{ return "wof"; },
        default_, []{ return "unknown"; }
    );
}

TEST(Switch, StringToString)
{
    ASSERT_EQ(voice("cat"), "mew");
    ASSERT_EQ(voice("dog"), "wof");
    ASSERT_EQ(voice("parrot"), "unknown");
    ASSERT_EQ(voice("mouse"), "unknown");
}

enum class Type { a, b };

static Type parseType(const QString& value)
{
    return switch_(value,
        "a", []{ return Type::a; },
        "b", []{ return Type::b; }
    );
}

TEST(Switch, StringToEnum)
{
    ASSERT_EQ(parseType("a"), Type::a);
    ASSERT_EQ(parseType("b"), Type::b);
    if (ini().assertCrash)
    {
        ASSERT_DEATH(parseType("c"), "Unmatched switch value");
        ASSERT_DEATH(parseType("d"), "Unmatched switch value");
    }
}

static QString toString(Type value)
{
    return NX_ENUM_SWITCH(value,
    {
        case Type::a: return "a";
        case Type::b: return "b";
    });
}

TEST(EnumSwitch, EnumToString)
{
    ASSERT_EQ(toString(Type::a), "a");
    ASSERT_EQ(toString(Type::b), "b");
    ASSERT_DEATH(toString(static_cast<Type>(42)), "Unmatched switch value: 42");
}

} // namespace test
} // namespace utils
} // namesapce nx
