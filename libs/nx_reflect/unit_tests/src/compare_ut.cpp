// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <string>

#include <gtest/gtest.h>

#include <nx/reflect/compare.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>


namespace nx::reflect::test {

struct Simple
{
    int i = 0;
    float f = 0.0;
    double d = 0.0;
    std::string s;
};
NX_REFLECTION_INSTRUMENT(Simple, (i)(f)(d)(s))

struct Float
{
    float f = 0.0f;
};
NX_REFLECTION_INSTRUMENT(Float, (f))

struct Double
{
    double d = 0.0;
};
NX_REFLECTION_INSTRUMENT(Double, (d))

TEST(Compare, equality)
{
    Simple f1{5, 1.0f, 0.0, "hello"};
    ASSERT_TRUE(nx::reflect::equals(f1, f1));

    ASSERT_FALSE(nx::reflect::equals(f1,
        Simple{6, 1.0f, 0.0, "hello"}));

    ASSERT_FALSE(nx::reflect::equals(f1,
        Simple{5, 2.0f, 0.0, "hello"}));

    ASSERT_FALSE(nx::reflect::equals(f1,
        Simple{5, 1.0f, 0.00000001, "hello"}));

    ASSERT_FALSE(nx::reflect::equals(f1,
        Simple{5, 1.0f, 0.0, "hello world"}));
}

TEST(Compare, fuzzyEqualityFloat)
{
    float f1 = 10000.0f;
    float f2 = 10000.1f;

    ASSERT_TRUE(nx::reflect::equals(Float{f1}, Float{f2}));

    Simple s1{5, f1, 0.0, "hello"};
    ASSERT_TRUE(nx::reflect::equals(s1, Simple{5, f2, 0.0, "hello"}));
}

TEST(Compare, fuzzyEqualityDouble)
{
    double d1 = 100000000000.0;
    double d2 = 100000000000.001;

    ASSERT_TRUE(nx::reflect::equals(Double{d1}, Double{d2}));

    Simple s1{5, 0.0f, d1, "hello"};
    ASSERT_TRUE(nx::reflect::equals(s1, Simple{5, 0.0f, d2, "hello"}));
}

TEST(Compare, fuzzyEqualityDoubleAfterDeserialized)
{
    Double value = {0.3};

    const auto serializedValue = nx::reflect::json::serialize(value);
    const auto [deserializedValue, result] =
        nx::reflect::json::deserialize<Double>(serializedValue);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(nx::reflect::equals(value, deserializedValue));
}

} // namespace nx::reflect::test
