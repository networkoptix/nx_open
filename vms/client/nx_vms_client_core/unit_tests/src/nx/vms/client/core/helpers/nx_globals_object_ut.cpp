// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/qml/nx_globals_object.h>

namespace nx::vms::client::core::test {

namespace {
bool checkSearchRegExpMatch(
    const QString& searchPattern,
    const QString& value)
{
    NxGlobalsObject globals;
    const auto searchRegExpPattern = globals.makeSearchRegExp(searchPattern);
    return QRegularExpression(searchRegExpPattern).match(value).hasMatch();
}

} // namespace

TEST(NxGlobalsObjectTest, SearchRegExp)
{
    static const auto kSingleCharacterString = QLatin1String("a");
    static const auto kNumbersTestString = QLatin1String("0123456789");
    static const auto kSpecialSymbols = QLatin1String("/\\!@#$^.,;:'\"{}`");

    ASSERT_TRUE(checkSearchRegExpMatch(kSingleCharacterString, kSingleCharacterString));
    ASSERT_TRUE(checkSearchRegExpMatch("?", kSingleCharacterString));
    ASSERT_TRUE(checkSearchRegExpMatch("*", kSingleCharacterString));
    ASSERT_TRUE(checkSearchRegExpMatch("0*9", kNumbersTestString));
    ASSERT_TRUE(checkSearchRegExpMatch("0*3*9", kNumbersTestString));
    ASSERT_TRUE(checkSearchRegExpMatch("0?2*", kNumbersTestString));
    ASSERT_TRUE(checkSearchRegExpMatch("0?2*9", kNumbersTestString));
    ASSERT_TRUE(checkSearchRegExpMatch("*3*", kNumbersTestString));
    ASSERT_TRUE(checkSearchRegExpMatch("*3?5*", kNumbersTestString));
    ASSERT_TRUE(checkSearchRegExpMatch("*7?9", kNumbersTestString));

    ASSERT_FALSE(checkSearchRegExpMatch("*4", kNumbersTestString));
    ASSERT_FALSE(checkSearchRegExpMatch("4*", kNumbersTestString));

    // Some checks for the regular expression special values.
    ASSERT_FALSE(checkSearchRegExpMatch(".", kNumbersTestString));
    ASSERT_FALSE(checkSearchRegExpMatch(".*", kNumbersTestString));

    // Some checks for the non-character symbols.
    for (const auto specialCharacter: kSpecialSymbols)
    {
        const auto searchString = QLatin1String("*%1*").arg(specialCharacter);
        ASSERT_TRUE(checkSearchRegExpMatch(searchString, kSpecialSymbols));
    }
}

} // namespace nx::vms::client::core::test
