#include <gtest/gtest.h>

#include <nx/network/http/server/rest/http_server_rest_path_matcher.h>

namespace nx::network::http::server::rest::test {

class RestPathMatcher:
    public ::testing::Test
{
protected:
    void assertPathRegistered(const std::string& restPath, int val)
    {
        ASSERT_TRUE(m_dictionary.add(restPath, val));
    }

    void assertPathNotRegistered(const std::string& restPath, int val)
    {
        ASSERT_FALSE(m_dictionary.add(restPath, val));
    }

    void assertPathMatches(
        const std::string& path,
        int expectedValue,
        RequestPathParams expectedParams)
    {
        RequestPathParams params;
        const auto result = m_dictionary.match(path, &params);

        ASSERT_TRUE(static_cast<bool>(result));
        ASSERT_EQ(expectedValue, *result);
        ASSERT_EQ(expectedParams, params);
    }

    void assertPathNotMatched(const std::string& path)
    {
        RequestPathParams params;
        ASSERT_FALSE(m_dictionary.match(path, &params));
    }

private:
    rest::PathMatcher<int> m_dictionary;
};

TEST_F(RestPathMatcher, find_handler)
{
    assertPathRegistered("/account/{accountId}/systems", 1);
    assertPathRegistered("/systems/{systemId}/users", 2);

    assertPathMatches("/account/vpupkin/systems", 1, {{{"accountId", "vpupkin"}}});
    assertPathMatches("/account/vpu-pk.i_n/systems", 1, {{{"accountId", "vpu-pk.i_n"}}});
    assertPathMatches("/systems/sys1/users", 2, {{{"systemId", "sys1"}}});
}

TEST_F(RestPathMatcher, find_handler_multiple_params)
{
    assertPathRegistered("/account/{accountId}/system/{systemName}/info", 3);

    assertPathMatches(
        "/account/akolesnikov/system/la_office_test/info",
        3,
        {{{"accountId", "akolesnikov"}, {"systemName", "la_office_test"}}});
}

TEST_F(RestPathMatcher, find_case_insensitive)
{
    assertPathRegistered("/account/{accountId}/systems", 1);
    assertPathMatches("/accOUNT/vpupkIN/SYSTems", 1, {{{"accountId", "vpupkIN"}}});
}

TEST_F(RestPathMatcher, no_suitable_handler)
{
    assertPathRegistered("/account/{accountId}/systems", 1);

    assertPathNotMatched("/account/vpupkin/info");
    assertPathNotMatched("/account/vpupkin/systems/");
    assertPathNotMatched("account/vpupkin/systems/");
    assertPathNotMatched("/account/systems");
}

TEST_F(RestPathMatcher, empty_parameter_value_is_not_accepted)
{
    assertPathRegistered("/account/{accountId}/systems", 1);
    assertPathNotMatched("/account//systems");
}

TEST_F(RestPathMatcher, registering_conflicting_handler)
{
    assertPathRegistered("/account/{accountId}/systems", 1);
    assertPathNotRegistered("/account/{accountId}/systems", 2);

    assertPathMatches("/account/vpupkin/systems", 1, {{{"accountId", "vpupkin"}}});
}

TEST_F(RestPathMatcher, path_with_same_name_attributes_cannot_be_registered)
{
    assertPathNotRegistered("/account/{accountId}/subAccount/{accountId}", 1);
}

TEST_F(RestPathMatcher, characters_supported_in_parameter_name)
{
    const std::string kParamName = 
        "abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789";

    assertPathRegistered("/account/{" + kParamName + "}/systems", 1);

    assertPathMatches(
        "/account/vpupkin/systems",
        1,
        {{{kParamName, "vpupkin"}}});
}

} // nx::network::http::server::rest::test
