#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/string.h>

namespace nx {
namespace utils {
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
        GroupToken::doubleQuotes | GroupToken::roundBraces | GroupToken::squareBraces);

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
        "n1=v1,n2=v2,n3=\"raz,raz,raz\"",
        {"n1=v1", "n2=v2", "n3=\"raz,raz,raz\""});
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

} // namespace test
} // namespace utils
} // namespace nx
