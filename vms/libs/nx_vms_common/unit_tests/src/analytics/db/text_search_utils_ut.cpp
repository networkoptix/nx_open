// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <analytics/common/object_metadata.h>

namespace nx::analytics::db::test {

TEST(AttributeEx, hasRange)
{
    using namespace common::metadata;

    NumericRange range(5.0);

    ASSERT_TRUE(range.hasRange(5.0));
    ASSERT_FALSE(range.hasRange(6.0));

    NumericRange range2 = AttributeEx::parseRangeFromValue("(4...6)");
    ASSERT_FALSE(range.hasRange(range2));
    ASSERT_TRUE(range2.hasRange(range));

    NumericRange range3 = AttributeEx::parseRangeFromValue("[4...6]");
    ASSERT_TRUE(range3.hasRange(range2));
    ASSERT_FALSE(range2.hasRange(range3));

}

TEST(AttributeEx, isNumericAttribute)
{
    using namespace common::metadata;

    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "1"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "15"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "2.5"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "2."));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", ".5"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "-15"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "-2.5"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "-2."));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "-.5"));

    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "abc"));
    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", ""));
    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "2abc"));
    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "a2b2c2"));
    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "abc2"));

    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "[1...10]"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "(1...10)"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "[1...10)"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "(1 ... 10]"));

    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "[110]"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "(1...10"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "1 ... 10)"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "1...10"));

    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "[1...inf]"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "[-inf...inf]"));
    ASSERT_TRUE(AttributeEx::isNumberOrRange("speed", "[-inf...1]"));

    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "[1...-inf]"));
    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "[inf...-inf]"));
    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "[inf...1]"));
    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "inf"));
    ASSERT_FALSE(AttributeEx::isNumberOrRange("speed", "-inf"));
}

} // namespace
