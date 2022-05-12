// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/dot_notation_string.h>

TEST(DotNotationString, basic)
{
    nx::utils::DotNotationString str;
    str.add("dot.notation.string");
    ASSERT_EQ(str.size(), 1);
    ASSERT_EQ(str.firstKey(), "dot");
    ASSERT_EQ(str.first().firstKey(), "notation");
    ASSERT_EQ(str.first().first().firstKey(), "string");
    ASSERT_TRUE(str["dot"]["notation"]["string"].empty());

    str.add("another.dot.notation.string");
    ASSERT_EQ(str.size(), 2);
    ASSERT_EQ(str.lastKey(), "dot");
    ASSERT_EQ(str.firstKey(), "another");
    ASSERT_EQ(str.first().firstKey(), "dot");
    ASSERT_EQ(str.first().first().firstKey(), "notation");
    ASSERT_EQ(str.first().first().first().firstKey(), "string");
    ASSERT_TRUE(str["another"]["dot"]["notation"]["string"].empty());

    str.add("dot.notation.string.more.parts");
    ASSERT_EQ(str.size(), 2);
    ASSERT_EQ(str["dot"].size(), 1);
    ASSERT_EQ(str["dot"]["notation"].size(), 1);
    ASSERT_EQ(str["dot"]["notation"]["string"].size(), 1);
    ASSERT_EQ(str["dot"]["notation"]["string"].firstKey(), "more");
    ASSERT_EQ(str["dot"]["notation"]["string"]["more"].firstKey(), "parts");
    ASSERT_TRUE(str["dot"]["notation"]["string"]["more"]["parts"].empty());
}

TEST(DotNotationString, find)
{
    nx::utils::DotNotationString str;
    str.add("simple.dot.notation.string");
    str.add("simple.dot.*.wildcard.string");

    ASSERT_NE(str.find("simple"), str.end());
    ASSERT_NE(str["simple"].find("dot"), str["simple"].end());
    ASSERT_EQ(str["simple"].find("dot").value().size(), 2);
    ASSERT_EQ(str["simple"]["dot"].find("nonExisting"), str["simple"]["dot"].end());
    ASSERT_NE(str["simple"]["dot"].findWithWildcard("nonExisting"), str["simple"]["dot"].end());
    ASSERT_EQ(str["simple"]["dot"].findWithWildcard("nonExisting").key(), "*");
    ASSERT_EQ(str["simple"]["dot"].findWithWildcard("nonExisting").value().firstKey(), "wildcard");
}

TEST(DotNotationString, accept)
{
    nx::utils::DotNotationString str;
    ASSERT_TRUE(str.accepts("anything"));

    str.add("simple.dot.*.wildcard.string");
    ASSERT_FALSE(str.accepts("anything"));
    ASSERT_TRUE(str.accepts("simple"));
    ASSERT_TRUE(str["simple"].accepts("dot"));
    ASSERT_TRUE(str["simple"]["dot"].accepts("nonExisting"));
    ASSERT_TRUE(str["simple"]["dot"]["*"].accepts("wildcard"));
    ASSERT_FALSE(str["simple"]["dot"]["*"].accepts("nonExisting"));
}
