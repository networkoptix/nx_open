// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/std_string_utils.h>

namespace nx::utils::test {

TEST(String, maxCommonPrefix)
{
    ASSERT_EQ("a", maxCommonPrefix(std::string("aabc"), std::string("a")));
    ASSERT_EQ("", maxCommonPrefix(std::string("abc"), std::string("def")));
    ASSERT_EQ("abc", maxCommonPrefix(std::string("abcdef"), std::string("abc")));
    ASSERT_EQ("", maxCommonPrefix(std::string(""), std::string("")));
    ASSERT_EQ("abc", maxCommonPrefix(std::string("abc"), std::string("abc")));

    ASSERT_EQ(
        std::vector<int>({ 1, 2, 3 }),
        maxCommonPrefix(std::vector<int>({ 1, 2, 3, 4, 5 }), std::vector<int>({ 1, 2, 3, 6, 7 })));
}

TEST(StringTrim, trimming_spaces)
{
    const std::vector<std::string> data = { " v1 ", "   ", "v2", "v3 ", " v4" };
    const std::vector<std::string> expected = { "v1", "", "v2", "v3", "v4" };

    auto actual = data;
    std::for_each(actual.begin(), actual.end(), [](auto& value) { trim(&value); });
    ASSERT_EQ(expected, actual);

    ASSERT_EQ("v1,", trim(" v1, "));
}

TEST(StringTrim, trimming_custom_characters)
{
    ASSERT_EQ("v1 ", trim("{v1 }", "{}"));
    ASSERT_EQ("v1", trim("--v1", "-"));
}

TEST(StringTrim, trim_inplace)
{
    std::string s("--v1");
    trim(&s, "-");
    ASSERT_EQ("v1", s);
}

//-------------------------------------------------------------------------------------------------

TEST(StringReplace, replaces)
{
    ASSERT_EQ("raz, raz, raz", replace("raz-raz-raz", "-", ", "));
    ASSERT_EQ("raz, raz, raz", replace(std::string("raz-raz-raz"), std::string("-"), std::string(", ")));
    ASSERT_EQ("raz, raz, raz", replace(std::string_view("raz-raz-raz"), std::string_view("-"), std::string_view(", ")));
    ASSERT_EQ("raz, raz, raz", replace(std::string("raz-raz-raz"), std::string_view("-"), std::string_view(", ")));
}

//-------------------------------------------------------------------------------------------------

class StringSplit:
    public ::testing::Test
{
protected:
    int m_groupTokens = GroupToken::none;
    int m_flags = SplitterFlag::skipEmpty;

    template<typename Separator>
    void assertSplitsAsExpected(
        const std::string& str,
        Separator sep,
        const std::vector<std::string_view>& expected)
    {
        std::vector<std::string_view> actual;
        split(str, sep, std::back_inserter(actual), m_groupTokens, m_flags);
        ASSERT_EQ(expected, actual);
    }
};

TEST_F(StringSplit, splits)
{
    assertSplitsAsExpected(
        "v1,v2, v3", separator::isAnyOf(","),
        { "v1", "v2", " v3" });

    assertSplitsAsExpected(
        "v1,v2, v3", separator::isAnyOf(std::string_view(",")),
        { "v1", "v2", " v3" });

    assertSplitsAsExpected(
        "v1,v2, v3",
        ',',
        { "v1", "v2", " v3" });
}

TEST_F(StringSplit, skipping_empty_tokens)
{
    assertSplitsAsExpected(
        "v1,v2,, v3", separator::isAnyOf(","),
        { "v1", "v2", " v3" });
}

TEST_F(StringSplit, providing_empty_tokens)
{
    m_flags = SplitterFlag::noFlags;

    assertSplitsAsExpected("v1,v2,, v3", ',', { "v1", "v2", "", " v3" });
    assertSplitsAsExpected("v1,v2", ',', { "v1", "v2" });
    assertSplitsAsExpected(",v1,", ',', { "", "v1", "" });
    assertSplitsAsExpected(",v1", ',', { "", "v1" });
    assertSplitsAsExpected("v1,", ',', { "v1", "" });
    assertSplitsAsExpected(",,,", ',', { "", "", "", "" });
    assertSplitsAsExpected("", ',', {});
}

TEST_F(StringSplit, empty_string)
{
    assertSplitsAsExpected(
        "", separator::isAnyOf(","),
        {});

    assertSplitsAsExpected(
        ",,,", separator::isAnyOf(","),
        {});
}

TEST_F(StringSplit, accepts_std_string)
{
    std::vector<std::string> tokens;
    nx::utils::split(std::string("raz,raz"), ',', std::back_inserter(tokens));
}

TEST_F(StringSplit, accepts_std_string_view)
{
    std::vector<std::string_view> tokens;
    nx::utils::split(std::string_view("raz,raz"), ',', std::back_inserter(tokens));
}

//-------------------------------------------------------------------------------------------------

class StringSplitN:
    public ::testing::Test
{
protected:
    template<int n, typename Separator>
    void assertSplitsAsExpected(
        const std::string& str,
        Separator sep,
        const std::array<std::string_view, n>& expectedTokens,
        int expectedCount)
    {
        const auto [tokens, count] = split_n<n>(str, sep, m_groupTokens, m_flags);
        ASSERT_EQ(expectedCount, count);
        ASSERT_EQ(expectedTokens, tokens);
    }

    void setGroupTokens(int val) { m_groupTokens = val; }
    void setFlags(int val) { m_flags = val; }

private:
    int m_groupTokens = GroupToken::none;
    int m_flags = SplitterFlag::skipEmpty;
};

TEST_F(StringSplitN, splits)
{
    assertSplitsAsExpected<2>(
        "abc:def", ':',
        std::array<std::string_view, 2>{"abc", "def"}, 2);

    assertSplitsAsExpected<2>(
        "a", ':',
        std::array<std::string_view, 2>{"a", ""}, 1);

    setFlags(SplitterFlag::noFlags);

    assertSplitsAsExpected<2>(
        "abc", ':',
        std::array<std::string_view, 2>{"abc", ""}, 1);
}

TEST_F(StringSplitN, reports_the_remainder_as_the_last_token)
{
    assertSplitsAsExpected<2>(
        "a:b:c:d:e:", ':',
        std::array<std::string_view, 2>{"a", "b:c:d:e:"}, 2);

    setGroupTokens(GroupToken::singleQuotes);

    assertSplitsAsExpected<2>(
        "a:b:'c:d':e:", ':',
        std::array<std::string_view, 2>{"a", "b:'c:d':e:"}, 2);

    assertSplitsAsExpected<2>(
        "a:'b:c:d:e:'", ':',
        std::array<std::string_view, 2>{"a", "'b:c:d:e:'"}, 2);

    assertSplitsAsExpected<2>(
        "a:'b:c:d:e:", ':',
        std::array<std::string_view, 2>{"a", "'b:c:d:e:"}, 2);
}

//-------------------------------------------------------------------------------------------------

class StringSplitQuoted:
    public StringSplit
{
public:
    StringSplitQuoted()
    {
        m_groupTokens = GroupToken::singleQuotes | GroupToken::doubleQuotes |
            GroupToken::roundBrackets | GroupToken::squareBrackets;
    }
};

TEST_F(StringSplitQuoted, separator_in_quotes_is_ignored)
{
    assertSplitsAsExpected(
        "n1=v1,n2=[v21,v22],n3=\"raz,raz,raz\",n4='x,y,z'",
        ',',
        { "n1=v1", "n2=[v21,v22]", "n3=\"raz,raz,raz\"", "n4='x,y,z'" });
}

TEST_F(StringSplitQuoted, separator_in_braces_is_ignored)
{
    assertSplitsAsExpected(
        "name=(\"\")",
        '=',
        { "name", "(\"\")" });

    assertSplitsAsExpected(
        "n1=v1,n2=[raz,raz],n3=\"raz,raz,raz\"",
        ',',
        { "n1=v1", "n2=[raz,raz]", "n3=\"raz,raz,raz\"" });

    assertSplitsAsExpected(
        "n1=v1,n2=(raz,\"raz,raz\",raz),n3=\"raz,[raz,raz],raz\"",
        ',',
        { "n1=v1", "n2=(raz,\"raz,raz\",raz)", "n3=\"raz,[raz,raz],raz\"" });
}

TEST_F(StringSplitQuoted, missing_closing_braces)
{
    assertSplitsAsExpected(
        "v=(abc=",
        '=',
        {"v", "(abc="});
}

TEST_F(StringSplitQuoted, other_group_token_inside_a_group_is_part_of_token)
{
    assertSplitsAsExpected(
        "v=(a=[b=d]c)",
        '=',
        {"v", "(a=[b=d]c)"});
}

TEST(StringSplitNameValuePairs, works_and_trims_value_of_quotes)
{
    std::map<std::string, std::string> params;
    const auto pairsFound = splitNameValuePairs(
        std::string("a=b, c= \"d,d,d\",e"), ',', '=',
        std::inserter(params, params.end()));

    const std::map<std::string, std::string> expected = { {"a", "b"}, {"c", "d,d,d"}, {"e", ""} };
    ASSERT_EQ(expected, params);
    ASSERT_EQ(expected.size(), pairsFound);
}

TEST(StringSplitNameValuePairs, accepts_string_view)
{
    std::map<std::string_view, std::string_view> params;
    splitNameValuePairs(
        std::string_view("a=b, c= \"d,d,d\",e"), ',', '=',
        std::inserter(params, params.end()));
}

//-------------------------------------------------------------------------------------------------

TEST(StringBuild, from_strings)
{
    ASSERT_EQ(
        std::string("raz,raz,raz"),
        buildString("raz", ",", std::string("raz"), ',', std::string_view("raz")));
}

TEST(StringBuild, from_integrals)
{
    ASSERT_EQ("counting: 1,2,100500", buildString("counting: ", 1, ',', 2, ',', 100500));
}

TEST(StringBuild, from_optional)
{
    std::optional<int> v;
    ASSERT_EQ("value: ", buildString("value: ", v));

    v = 101;
    ASSERT_EQ("value: 101", buildString("value: ", v));

    std::optional<std::string> s;
    ASSERT_EQ("str: ", buildString("str: ", s));

    s = "raz";
    ASSERT_EQ("str: raz", buildString("str: ", s));
}

TEST(StringBuild, supports_buffer)
{
    ASSERT_EQ(
        "raz, raz, raz!",
        nx::utils::buildString<BasicBuffer<char>>("raz, ", BasicBuffer<char>("raz, "), "raz!"));
}

TEST(StringBuild, supports_any_type_with_data_and_size)
{
    struct Raz
    {
        const char* data() const { return "raz"; };
        std::size_t size() const { return std::strlen("raz"); }
    };

    auto x = BasicBuffer<char>("raz, ");
    x[0] = '\0';
    auto y = QByteArray("raz!");
    y[1] = '\0';

    ASSERT_EQ(
        BasicBuffer<char>(Raz().data()) + x + BasicBuffer<char>(y),
        nx::utils::buildString(Raz(), x, y));
}

//-------------------------------------------------------------------------------------------------

struct Record
{
    std::string a;
    std::string b;
    int x = -1;

    const std::string& getA() const { return a; };

    std::string toString() const
    {
        return nx::utils::buildString(
            "a=", getA(), ',', "b=", b, ',', "x=", x);
    }
};

class StringJoinRecords:
    public ::testing::Test
{
protected:
    static const std::vector<std::optional<Record>> tokens;

    static constexpr char kExpected[] = "a=a1,b=b1,x=12;a=a2,b=b2,x=34";
};

const std::vector<std::optional<Record>> StringJoinRecords::tokens =
    { Record{"a1", "b1", 12}, std::nullopt, Record{"a2", "b2", 34} };

// The most efficient (in terms of allocation count) syntax.
// As low as a single allocation if each element string representation is short.
TEST_F(StringJoinRecords, the_most_efficient_syntax)
{
    {
        const auto actual = join(
            tokens.begin(), tokens.end(),
            ';',
            [](std::string* out, const auto& rec)
            {
                nx::utils::buildString(
                    out,
                    "a=", rec->getA(), ',', "b=", rec->b, ',', "x=", rec->x);
            });

        ASSERT_EQ(kExpected, actual);
    }

    {
        const auto actual = join(
            tokens,
            ';',
            [](std::string* out, const auto& rec)
            {
                nx::utils::buildString(
                    out,
                    "a=", rec->getA(), ',', "b=", rec->b, ',', "x=", rec->x);
            });

        ASSERT_EQ(kExpected, actual);
    }
}

// Convenience overload: Record::toString() is invoked implicitly.
TEST_F(StringJoinRecords, convenience_overload_1)
{
    ASSERT_EQ(kExpected, join(tokens.begin(), tokens.end(), ';'));
    ASSERT_EQ(kExpected, join(tokens, ';'));
}

// Convenience overload: as if Record does not have toString() method.
TEST_F(StringJoinRecords, convenience_overload_2)
{
    ASSERT_EQ(
        kExpected,
        join(
            tokens.begin(), tokens.end(),
            ';',
            [](const auto& rec) { return rec->toString(); }));

    ASSERT_EQ(
        kExpected,
        join(tokens, ';', [](const auto& rec) { return rec->toString(); }));
}

// Joining range of iterators that have linear time distance calculation.
TEST_F(StringJoinRecords, range_with_linear_distance_complexity)
{
    std::list<decltype(tokens)::value_type> tokens1;
    std::copy(tokens.begin(), tokens.end(), std::back_inserter(tokens1));

    const auto actual = join(
        tokens1.begin(), tokens1.end(),
        ';',
        [](const auto& rec) { return rec->toString(); });

    ASSERT_EQ(kExpected, actual);
}

TEST(StringJoin, strings)
{
    const std::vector<std::string> data = { "abc", "def", "ghi" };

    const auto actual = join(data.begin(), data.end(), ',');
    ASSERT_EQ("abc,def,ghi", actual);
}

TEST(StringJoin, optional_strings)
{
    const std::vector<std::optional<std::string>> data = { "abc", "def", std::nullopt, "ghi" };

    const auto actual = join(data.begin(), data.end(), ',');
    ASSERT_EQ("abc,def,ghi", actual);
}

TEST(StringJoin, string_views)
{
    const std::vector<std::string_view> data = { "abc", "def", "ghi" };

    const auto actual = join(data.begin(), data.end(), ',');
    ASSERT_EQ("abc,def,ghi", actual);
}

//-------------------------------------------------------------------------------------------------

TEST(String, endsWith)
{
    ASSERT_TRUE(endsWith(std::string("one,two"), "two"));
    ASSERT_TRUE(endsWith(std::string("one,two"), std::string("two")));
    ASSERT_TRUE(endsWith(std::string("one,two"), std::string_view("two")));
    ASSERT_TRUE(endsWith(std::string("one,two"), 'o'));
    ASSERT_TRUE(endsWith(std::string("one,two"), ""));
    ASSERT_TRUE(endsWith(std::string_view("one,two"), std::string_view("two")));

    ASSERT_FALSE(endsWith(std::string("one"), "E"));
    ASSERT_FALSE(endsWith(std::string("one"), "two,three"));
    ASSERT_FALSE(endsWith(std::string("one,two"), "three"));
    ASSERT_FALSE(endsWith(std::string(), "three"));
    ASSERT_FALSE(endsWith(std::string(), 'h'));

    ASSERT_TRUE(endsWith(std::string("one,two"), "tWo", CaseSensitivity::off));
    ASSERT_FALSE(endsWith(std::string("one,two"), "one", CaseSensitivity::off));
}

TEST(String, startsWith)
{
    ASSERT_TRUE(startsWith(std::string("one,two"), "one"));
    ASSERT_TRUE(startsWith(std::string("one,two"), std::string("one")));
    ASSERT_TRUE(startsWith(std::string("one,two"), std::string_view("one")));
    ASSERT_TRUE(startsWith(std::string("one,two"), 'o'));
    ASSERT_TRUE(startsWith(std::string("one,two"), ""));
    ASSERT_TRUE(startsWith(std::string_view("one,two"), std::string_view("one")));

    ASSERT_FALSE(startsWith(std::string("one"), "two,three"));
    ASSERT_FALSE(startsWith(std::string("one,two"), "three"));
    ASSERT_FALSE(startsWith(std::string(), "three"));
    ASSERT_FALSE(startsWith(std::string(), 'h'));

    ASSERT_TRUE(startsWith(std::string("one,two"), "ONE", CaseSensitivity::off));
    ASSERT_FALSE(startsWith(std::string("one,two"), "two", CaseSensitivity::off));
}

//-------------------------------------------------------------------------------------------------

TEST(String, stricmp)
{
    ASSERT_EQ(0, stricmp("ABC", "abc"));
    ASSERT_LT(stricmp("ABC", "bbc"), 0);
    ASSERT_GT(stricmp("bbc", "ABC"), 0);

    ASSERT_LT(stricmp("abcd", "ABCDEF"), 0);
    ASSERT_GT(stricmp("ABCDEF", "abcd"), 0);
    ASSERT_LT(stricmp("", "abcd"), 0);

    ASSERT_EQ(0, stricmp("", ""));
}

//-------------------------------------------------------------------------------------------------
// reverseWords

TEST(StringReverseWords, common)
{
    ASSERT_EQ("com.domain.test", reverseWords(std::string_view("test.domain.com"), '.'));
    ASSERT_EQ(".com...example.test.", reverseWords(std::string_view(".test.example...com."), '.'));
    ASSERT_EQ(".com", reverseWords(std::string_view("com."), '.'));
    ASSERT_EQ("com.", reverseWords(std::string_view(".com"), '.'));
    ASSERT_EQ(".", reverseWords(std::string_view("."), '.'));
    ASSERT_EQ("", reverseWords(std::string_view(""), '.'));
}

TEST(StringReverseWords, empty_string)
{
    ASSERT_EQ("", reverseWords(std::string_view(), '.'));
}

TEST(StringReverseWords, no_separators)
{
    ASSERT_EQ("com", reverseWords(std::string_view("com"), '.'));
}

TEST(StringReverseWords, only_separators)
{
    ASSERT_EQ("...", reverseWords(std::string_view("..."), '.'));
    ASSERT_EQ(".", reverseWords(std::string_view("."), '.'));
}

//-------------------------------------------------------------------------------------------------
// reverseWords

TEST(StringRemoveExcessSeparators, common)
{
    ASSERT_EQ("Hello world", removeExcessSeparators(std::string_view("   Hello  world "), ' '));
    ASSERT_EQ("Hello world", removeExcessSeparators(std::string_view("Hello world"), ' '));
    ASSERT_EQ("", removeExcessSeparators(std::string_view(""), ' '));
    ASSERT_EQ("", removeExcessSeparators(std::string_view(" "), ' '));
    ASSERT_EQ("", removeExcessSeparators(std::string_view("  "), ' '));
}

//-------------------------------------------------------------------------------------------------
// Hex

class StringHex:
    public ::testing::Test
{
protected:
    void assertReversible(const std::string& data, const std::string& hex)
    {
        ASSERT_EQ(hex, toHex(data));
        ASSERT_EQ(data, fromHex(hex));
    }
};

TEST_F(StringHex, common)
{
    assertReversible("1234", "31323334");
    assertReversible("", "");
    assertReversible(std::string("\0\1\2\3", 4), "00010203");
}

TEST_F(StringHex, int_to_hex)
{
    ASSERT_EQ("b", toHex(11U));
    ASSERT_EQ("0", toHex(0U));
    ASSERT_EQ("5d", toHex(93U));
    ASSERT_EQ("7048860ddf79", toHex(123456789012345ULL));
}

TEST_F(StringHex, invalidHex)
{
    ASSERT_EQ("", fromHex("xyz"));
    ASSERT_EQ("12", fromHex("3132xyz"));
    ASSERT_EQ("1234", fromHex("3132xyz3334"));
}

//-------------------------------------------------------------------------------------------------
// Base64
// TODO: Add tests when introducing own implementation.

} // namespace nx::utils::test
