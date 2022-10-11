// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
        RequestPathParams expectedParams,
        const std::string& expectedPathTemplate)
    {
        const auto result = m_dictionary.match(path);

        ASSERT_TRUE(static_cast<bool>(result));
        ASSERT_EQ(expectedValue, result->value);
        ASSERT_EQ(expectedParams, result->pathParams);
        ASSERT_EQ(expectedPathTemplate, result->pathTemplate);
    }

    void assertPathNotMatched(const std::string& path)
    {
        ASSERT_FALSE(m_dictionary.match(path));
    }

private:
    rest::PathMatcher<int> m_dictionary;
};

TEST_F(RestPathMatcher, find_handler)
{
    const std::string systems{"/account/{accountId}/systems"};
    const std::string users{"/systems/{systemId}/users"};
    assertPathRegistered(systems, 1);
    assertPathRegistered(users, 2);

    assertPathMatches("/account/vpupkin/systems", 1, {{"accountId", "vpupkin"}}, systems);
    assertPathMatches("/account/vpu-pk.i_n/systems", 1, {{"accountId", "vpu-pk.i_n"}}, systems);
    assertPathMatches("/systems/sys1/users", 2, {{"systemId", "sys1"}}, users);
}

TEST_F(RestPathMatcher, find_handler_multiple_params)
{
    const std::string info{"/account/{accountId}/system/{systemName}/info"};
    assertPathRegistered(info, 3);

    assertPathMatches(
        "/account/akolesnikov/system/la_office_test/info",
        3,
        {{"accountId", "akolesnikov"}, {"systemName", "la_office_test"}}, info);
}

TEST_F(RestPathMatcher, find_case_insensitive)
{
    const std::string systems{"/account/{accountId}/systems"};
    assertPathRegistered(systems, 1);
    assertPathMatches("/accOUNT/vpupkIN/SYSTems", 1, {{"accountId", "vpupkIN"}}, systems);
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
    const std::string systems{"/account/{accountId}/systems"};
    assertPathRegistered(systems, 1);
    assertPathNotRegistered("/account/{accountId}/systems", 2);

    assertPathMatches("/account/vpupkin/systems", 1, {{"accountId", "vpupkin"}}, systems);
}

TEST_F(RestPathMatcher, path_with_same_name_attributes_cannot_be_registered)
{
    assertPathNotRegistered("/account/{accountId}/subAccount/{accountId}", 1);
}

TEST_F(RestPathMatcher, characters_supported_in_parameter_name)
{
    const std::string kParamName =
        "abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789";

    const std::string systemsWithBigParam = "/account/{" + kParamName + "}/systems";
    assertPathRegistered(systemsWithBigParam, 1);

    assertPathMatches(
        "/account/vpupkin/systems",
        1,
        {{kParamName, "vpupkin"}}, systemsWithBigParam);
}

TEST_F(RestPathMatcher, sub_resource_path_is_not_matched_as_parent_resource_name)
{
    assertPathRegistered("/account/{accountId}", 1);
    assertPathRegistered("/account/{accountId}/bills/", 2);

    assertPathNotMatched("/account/vpupkin/systems");
    assertPathMatches("/account/vpupkin", 1, {{"accountId", "vpupkin"}}, "/account/{accountId}");
    assertPathMatches("/account/vpupkin/bills/", 2, {{"accountId", "vpupkin"}}, "/account/{accountId}/bills/");
}

} // nx::network::http::server::rest::test
