// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/server/wildcard_path_matcher.h>

namespace nx::network::http::test {

class HttpWildcardPathMatcher:
    public ::testing::Test
{
protected:
    void add(const std::string_view& expr)
    {
        ASSERT_TRUE(m_matcher.add(expr, std::string(expr)));
    }

    void assertMatch(const std::string_view& path, const std::string& expected)
    {
        const auto matchResult = m_matcher.match(path);
        ASSERT_TRUE(matchResult);
        ASSERT_EQ(expected, matchResult->value);
    }

    void assertNoMatch(const std::string_view& path)
    {
        const auto matchResult = m_matcher.match(path);
        ASSERT_FALSE(matchResult);
    }

private:
    WildcardPathMatcher<std::string> m_matcher;
};

TEST_F(HttpWildcardPathMatcher, match)
{
    add("*");
    add("/foo/one/bar");
    add("/foo/*");
    add("/foo/?/bar");
    add("/foo/*/bar");
    add("/foo");

    assertMatch("/example", "*");
    assertMatch("/foo/one/bar", "/foo/one/bar");
    assertMatch("/foo/two", "/foo/*");
    assertMatch("/foo/some/bar", "/foo/*/bar");
    assertMatch("/foo/q/bar", "/foo/?/bar");
    assertMatch("/foo", "/foo");
    assertMatch("/foo1", "*");
}

TEST_F(HttpWildcardPathMatcher, no_match)
{
    add("/foo/one/bar");
    add("/bar/*");
    add("/foo");

    assertNoMatch("/foo1");
    assertNoMatch("/foo/one/bar/smth");
    assertNoMatch("/bar");
    assertNoMatch("/bar1");
}

} // namespace nx::rnetwork::http::test
