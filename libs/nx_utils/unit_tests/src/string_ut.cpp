#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/string.h>

namespace nx::utils {
namespace test {

namespace {

void assertStringSplitAsExpected(
    const char* str,
    std::vector<const char*> expectedTokens)
{
    QByteArray input(str);
    const auto tokens = splitQuotedString(
        input,
        ',',
        GroupToken::singleQuotes | GroupToken::doubleQuotes |
            GroupToken::roundBraces | GroupToken::squareBraces);

    ASSERT_EQ(expectedTokens.size(), tokens.size());
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        ASSERT_EQ(expectedTokens[i], tokens[i]);
    }
}

} // namespace

TEST(splitQuotedString, separator_in_quotes_is_ignored)
{
    assertStringSplitAsExpected(
        "n1=v1,n2=[v21,v22],n3=\"raz,raz,raz\",n4='x,y,z'",
        {"n1=v1", "n2=[v21,v22]", "n3=\"raz,raz,raz\"", "n4='x,y,z'"});
}

TEST(splitQuotedString, separator_in_braces_is_ignored)
{
    assertStringSplitAsExpected(
        "n1=v1,n2=[raz,raz],n3=\"raz,raz,raz\"",
        {"n1=v1", "n2=[raz,raz]", "n3=\"raz,raz,raz\""});

    assertStringSplitAsExpected(
        "n1=v1,n2=(raz,\"raz,raz\",raz),n3=\"raz,[raz,raz],raz\"",
        {"n1=v1", "n2=(raz,\"raz,raz\",raz)", "n3=\"raz,[raz,raz],raz\""});
}

//-------------------------------------------------------------------------------------------------

TEST(String, maxPrefix)
{
    ASSERT_EQ("a", maxPrefix(std::string("aabc"), std::string("a")));
    ASSERT_EQ("", maxPrefix(std::string("abc"), std::string("def")));
    ASSERT_EQ("abc", maxPrefix(std::string("abcdef"), std::string("abc")));
    ASSERT_EQ("", maxPrefix(std::string(""), std::string("")));
    ASSERT_EQ("abc", maxPrefix(std::string("abc"), std::string("abc")));

    ASSERT_EQ(
        std::vector<int>({1, 2, 3}),
        maxPrefix(std::vector<int>({1, 2, 3, 4, 5}), std::vector<int>({1, 2, 3, 6, 7})));
}

TEST(replaceStrings, emptySubstitutionsLeaveIntact)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {}),
        QString("abc"));
}

TEST(replaceStrings, noMatch)
{
    ASSERT_EQ(replaceStrings(
        "def",
        {{"a", "b"}}),
        QString("def"));
}

TEST(replaceStrings, independentSubstitutions)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"a", "d"}, {"c", "e"}}),
        QString("dbe"));
}

TEST(replaceStrings, sequentiveSubstitutions)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"a", "b"}, {"b", "c"}, {"c", "a"}}),
        QString("bca"));
}

TEST(replaceStrings, sequentiveSubstitutionsOfDifferentSize)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"a", "abc"}, {"b", "abc"}, {"c", "abc"}}),
        QString("abcabcabc"));
}

TEST(replaceStrings, sequentiveSubstitutionsWithOverriding)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"ab", "b"}, {"b", "c"}}),
        QString("bc"));
}

TEST(replaceStrings, substitutionStartsWithSubstringOfAnotherSubstitution)
{
    ASSERT_EQ(replaceStrings(
        "abac",
        {{"ab", "b"}, {"a", "c"}}),
        QString("bcc"));
}

TEST(replaceStrings, orderDoesMatter)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"a", "b"}, {"a", "c"}}),
        QString("bbc"));
}

TEST(replaceStrings, specialCharacters)
{
    ASSERT_EQ(replaceStrings(
        "|.*",
        {{".", "*"}, {"|", "*"}}),
        QString("***"));
}

} // namespace test
} // namespace nx::utils
