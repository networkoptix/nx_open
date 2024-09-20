// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/enum_string_conversion.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>

using namespace std::literals::string_view_literals;

enum class SimpleEnum
{
    one, two, three
};
NX_REFLECTION_INSTRUMENT_ENUM(SimpleEnum, one, two, three)

TEST(Enum, simple)
{
    EXPECT_EQ("three", nx::reflect::toString(SimpleEnum::three));

    auto v = SimpleEnum::one;
    nx::reflect::fromString("two"sv, &v);
    EXPECT_EQ(v, SimpleEnum::two);

    auto allEnumValues = nx::reflect::enumeration::allEnumValues<SimpleEnum>();
    EXPECT_EQ(allEnumValues.size(), 3);
    EXPECT_NE(std::ranges::find(allEnumValues, SimpleEnum::one), allEnumValues.end());
    EXPECT_NE(std::ranges::find(allEnumValues, SimpleEnum::two), allEnumValues.end());
    EXPECT_NE(std::ranges::find(allEnumValues, SimpleEnum::three), allEnumValues.end());
    EXPECT_EQ(std::ranges::find(allEnumValues, (SimpleEnum) 4), allEnumValues.end());
}

NX_REFLECTION_ENUM_CLASS(DefinedSimpleEnum, one, two, three)
TEST(Enum, definedSimple)
{
    EXPECT_EQ("three", nx::reflect::toString(DefinedSimpleEnum::three));

    auto v = DefinedSimpleEnum::one;
    nx::reflect::fromString("two"sv, &v);
    EXPECT_EQ(v, DefinedSimpleEnum::two);
}

NX_REFLECTION_ENUM_CLASS(DefinedEnumWithValues, a, b = 15, c)
TEST(Enum, definedWithValues)
{
    EXPECT_EQ((int) DefinedEnumWithValues::b, 15);
    EXPECT_EQ("b", nx::reflect::toString(DefinedEnumWithValues::b));

    auto v = DefinedEnumWithValues::a;
    nx::reflect::fromString("b"sv, &v);
    EXPECT_EQ(v, DefinedEnumWithValues::b);
}

TEST(Enum, deserializationIsCaseInsensitive)
{
    auto v = SimpleEnum::one;

    EXPECT_TRUE(nx::reflect::fromString("two", &v));
    EXPECT_EQ(v, SimpleEnum::two);

    v = SimpleEnum::one;
    EXPECT_TRUE(nx::reflect::fromString("Two", &v));
    EXPECT_EQ(v, SimpleEnum::two);
}

struct Enums
{
    SimpleEnum v1;
    DefinedSimpleEnum v2;
    DefinedEnumWithValues v3;

    bool operator==(const Enums& other) const
    {
        return v1 == other.v1 && v2 == other.v2 && v3 == other.v3;
    }
};
NX_REFLECTION_INSTRUMENT(Enums, (v1)(v2)(v3))

TEST(Enum, structWithEnumsSerialization)
{
    const Enums enums{SimpleEnum::three, DefinedSimpleEnum::two, DefinedEnumWithValues::a};

    const auto serialized = nx::reflect::json::serialize(enums);
    const auto expected = R"json({"v1":"three","v2":"two","v3":"a"})json";
    EXPECT_EQ(serialized, expected);
}

TEST(Enum, structWithEnumsDeserialization)
{
    const auto serialized = R"json({"v1":"three","v2":"two","v3":"a"})json";
    const Enums expected{SimpleEnum::three, DefinedSimpleEnum::two, DefinedEnumWithValues::a};

    Enums enums;
    ASSERT_TRUE(nx::reflect::json::deserialize(serialized, &enums));
    EXPECT_EQ(enums, expected);
}

NX_REFLECTION_ENUM_CLASS(TrailingUnderscores, custom, default_)
TEST(Enum, TrailingUnderscores)
{
    EXPECT_EQ("default", nx::reflect::toString(TrailingUnderscores::default_));

    auto v = TrailingUnderscores::custom;
    nx::reflect::fromString("default"sv, &v);
    EXPECT_EQ(v, TrailingUnderscores::default_);
}

enum class CustomNames { one, two, three };
template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(CustomNames*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<CustomNames>;
    return visitor(
        Item{CustomNames::one, "ONE"},
        Item{CustomNames::two, "TWO"},
        Item{CustomNames::three, "THREE"}
    );
}

TEST(Enum, customNames)
{
    EXPECT_EQ("TWO", nx::reflect::toString(CustomNames::two));

    auto v = CustomNames::one;
    nx::reflect::fromString("THREE"sv, &v);
    EXPECT_EQ(v, CustomNames::three);
}

enum class MultipleNames { one, two, three };
template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(MultipleNames*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<MultipleNames>;
    return visitor(
        Item{MultipleNames::one, "one"},
        Item{MultipleNames::one, "ONE"},
        Item{MultipleNames::two, "two"},
        Item{MultipleNames::two, "TWO"},
        Item{MultipleNames::three, "three"},
        Item{MultipleNames::three, "THREE"}
    );
}

TEST(Enum, multipleNamesForItem)
{
    EXPECT_EQ("two", nx::reflect::toString(MultipleNames::two));
    EXPECT_EQ(MultipleNames::three, nx::reflect::fromString<MultipleNames>("three"));
    EXPECT_EQ(MultipleNames::three, nx::reflect::fromString<MultipleNames>("THREE"));
}

NX_REFLECTION_ENUM_CLASS(SameValues, x = 1, y = 2, z = SameValues::x)

TEST(Enum, sameValues)
{
    EXPECT_EQ("x", nx::reflect::toString(SameValues::x));
    EXPECT_EQ("x", nx::reflect::toString(SameValues::z));
    EXPECT_EQ(SameValues::x, nx::reflect::fromString<SameValues>("x"));
    EXPECT_EQ(SameValues::x, nx::reflect::fromString<SameValues>("z"));
}

NX_REFLECTION_ENUM_CLASS(NumericEnum, x = 5, y = 10, z = 15)

TEST(Enum, numericSerialization)
{
    EXPECT_EQ(nx::reflect::enumeration::toString(NumericEnum::x), "x");
    EXPECT_EQ(nx::reflect::enumeration::toString(static_cast<NumericEnum>(11)), "11");
}

TEST(Enum, numericDeserialization)
{
    auto value = NumericEnum::x;
    EXPECT_TRUE(nx::reflect::enumeration::fromString("10", &value));
    EXPECT_EQ(NumericEnum::y, value);

    EXPECT_TRUE(nx::reflect::enumeration::fromString("2", &value));
    EXPECT_EQ(static_cast<NumericEnum>(2), value);

    EXPECT_TRUE(nx::reflect::enumeration::fromString("0xf", &value));
    EXPECT_EQ(NumericEnum::z, value);

    EXPECT_TRUE(nx::reflect::enumeration::fromString("0XB", &value));
    EXPECT_EQ(static_cast<NumericEnum>(11), value);
}

struct StructWithEnum
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(SimpleEnum, one, two, three)
    SimpleEnum v = SimpleEnum::two;
};
NX_REFLECTION_INSTRUMENT(StructWithEnum, (v))

TEST(Enum, enumInsideStruct)
{
    EXPECT_EQ("two", nx::reflect::toString(StructWithEnum::SimpleEnum::two));

    auto v = StructWithEnum::SimpleEnum::one;
    nx::reflect::fromString("three"sv, &v);
    EXPECT_EQ(StructWithEnum::SimpleEnum::three, v);

    StructWithEnum s{StructWithEnum::SimpleEnum::two};
    const auto serialized = nx::reflect::json::serialize(s);
    const auto expected = R"json({"v":"two"})json";
    EXPECT_EQ(serialized, expected);
}
