// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/server/http_server_exact_path_matcher.h>
#include <nx/network/http/server/request_matcher.h>

namespace nx::network::http {

class HttpRequestMatcher:
    public ::testing::Test
{
protected:
    void add(const Method& method, const std::string& path, int val)
    {
        m_matcher.add(method, path, val);
    }

    void assertMatched(const Method& method, const std::string& path, int expected)
    {
        const auto result = m_matcher.match(method, path);
        ASSERT_TRUE(result);
        ASSERT_EQ(expected, result->value);
    }

    void assertNotMatched(const Method& method, const std::string& path)
    {
        const auto result = m_matcher.match(method, path);
        ASSERT_FALSE(result) << result->pathTemplate;
    }

private:
    using MatchResult = typename RequestMatcher<int, ExactPathMatcher>::MatchResult;

    RequestMatcher<int, ExactPathMatcher> m_matcher;
};

TEST_F(HttpRequestMatcher, match)
{
    add(Method::get, "/foo/bar", 1);

    // any method, specific path
    add("", "/foo/bar", 2);

    // Default handler for GET method.
    add(Method::get, "", 3);

    //---------------------------------------------------------------------------------------------

    assertMatched(Method::get, "/foo/bar", 1);
    assertMatched(Method::put, "/foo/bar", 2);
    assertNotMatched(Method::put, "/foo/bar/1");
    assertMatched(Method::get, "/bar", 3);

    //---------------------------------------------------------------------------------------------

    // Default handler.
    add("", "", 101);

    assertMatched(Method::delete_, "/bar", 101);
}

} // namespace nx::network::http
