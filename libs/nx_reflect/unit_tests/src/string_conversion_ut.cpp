// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <nx/reflect/from_string.h>
#include <nx/reflect/to_string.h>

// Not using nx::reflect namespace for the honest testing of function search through the ADL.
namespace nx_reflect_detail_test {

namespace {

class Base64Convertible
{
public:
    std::string toBase64() const { return ""; }
    std::string toBase64(int /*dummyOptions*/) const { return ""; }
};

class NotBase64Convertible {};

} // namespace

TEST(StringizeBase64Convertible, HasToBase64V)
{
    static_assert(
        nx::reflect::detail::HasToBase64V<Base64Convertible>,
        "HasToBase64V<Base64Convertible>");

    static_assert(
        !nx::reflect::detail::HasToBase64V<NotBase64Convertible>,
        "!HasToBase64V<NotBase64Convertible>");
}

//-------------------------------------------------------------------------------------------------

struct CustomStringizable
{
    std::string text;

    std::string toString() const { return text; }

    static CustomStringizable fromString(const std::string_view& s)
    {
        CustomStringizable result;
        result.text = (std::string) s;
        return result;
    }

    bool operator==(const CustomStringizable& right) const { return text == right.text; }
};

TEST(Stringize, in_class_fromString_is_used)
{
    CustomStringizable s{"Hello, reflect"};

    ASSERT_EQ(s.text, nx::reflect::toString(s));

    ASSERT_EQ(s, nx::reflect::fromString<CustomStringizable>((std::string_view) s.text));
    ASSERT_EQ(s, nx::reflect::fromString<CustomStringizable>(s.text));
    ASSERT_EQ(s, nx::reflect::fromString<CustomStringizable>(s.text.c_str()));
}

TEST(Duration, DurationToString)
{
    std::chrono::seconds seconds{120};
    std::chrono::minutes minutes{120};

    ASSERT_EQ("120", nx::reflect::toString(seconds));
    ASSERT_EQ("120", nx::reflect::toString(minutes));
}

} // namespace nx_reflect_detail_test
