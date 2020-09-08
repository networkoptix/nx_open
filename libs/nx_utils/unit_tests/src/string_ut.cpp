#include <vector>

#include <gtest/gtest.h>

#include <array>
#include <list>
#include <array>
#include <set>

#include <QtCore/QList>
#include <QtCore/QVector>

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

//-------------------------------------------------------------------------------------------------

TEST(String, trimAndUnquote)
{
    ASSERT_EQ("foo", nx::utils::trimAndUnquote(QString("\"foo\"")));
    ASSERT_EQ("foo", nx::utils::trimAndUnquote(QString("foo")));
    ASSERT_EQ("foo", nx::utils::trimAndUnquote(QString("\"foo")));
    ASSERT_EQ("foo", nx::utils::trimAndUnquote(QString("foo\"")));

    ASSERT_EQ("", nx::utils::trimAndUnquote(QString()));
    ASSERT_EQ("", nx::utils::trimAndUnquote(QString("\"\"")));
    ASSERT_EQ("", nx::utils::trimAndUnquote(QString("\"")));
}

//-------------------------------------------------------------------------------------------------

TEST(String, join)
{
    std::vector<QString> source1{"s1", "s2", "s3"};
    ASSERT_EQ(nx::utils::join(source1, ", "), QString("s1, s2, s3"));
    ASSERT_EQ(nx::utils::join(source1.begin() + 1, source1.end(), ", "), QString("s2, s3"));

    std::list<QByteArray>source2{"s1", "s2", "s3"};
    ASSERT_EQ(nx::utils::join(source2, QByteArray()), QByteArray("s1s2s3"));

    std::set<std::string> source3{"s2", "s3", "s1"};
    ASSERT_EQ(nx::utils::join(source3, "   "), std::string("s1   s2   s3"));
    ASSERT_EQ(nx::utils::join(source3.rbegin(), source3.rend(), " "), std::string("s3 s2 s1"));

    std::array<QString, 3> source4{"s1", "s2", "s3"};
    ASSERT_EQ(nx::utils::join(source4, QString("-")), QString("s1-s2-s3"));

    QVector<QString> source5{"s1"};
    ASSERT_EQ(nx::utils::join(source5, ' '), QString("s1"));

    std::vector<std::string> source6{"s1", "", "s2", "", ""};
    ASSERT_EQ(nx::utils::join(source6, std::string(",")), std::string("s1,,s2,,"));

    QList<std::string> source7;
    ASSERT_EQ(nx::utils::join(source7, std::string()), std::string());
}

} // namespace test
} // namespace nx::utils
