#include <gtest/gtest.h>

#include <nx/network/http/server/rest/http_server_rest_path_matcher.h>

namespace nx_http {
namespace server {
namespace rest {
namespace test {

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
        std::vector<std::string> expectedParams)
    {
        std::vector<std::string> params;
        const auto& result = m_dictionary.match(path, &params);

        ASSERT_TRUE(result);
        ASSERT_EQ(expectedValue, *result);
        ASSERT_EQ(expectedParams.size(), params.size());
        for (std::size_t i = 0; i < expectedParams.size(); ++i)
        {
            ASSERT_EQ(expectedParams[i], params[i]);
        }
    }

    void assertPathNotMatched(const std::string& path)
    {
        std::vector<std::string> params;
        ASSERT_FALSE(m_dictionary.match(path, &params));
    }

private:
    rest::RestPathMatcher<int> m_dictionary;
};

TEST_F(RestPathMatcher, find_handler)
{
    assertPathRegistered("/account/{accountId}/systems", 1);
    assertPathRegistered("/systems/{systemId}/users", 2);

    assertPathMatches("/account/vpupkin/systems", 1, { "vpupkin" });
    assertPathMatches("/account/vpu-pk.i_n/systems", 1, { "vpu-pk.i_n" });
    assertPathMatches("/systems/sys1/users", 2, { "sys1" });
}

TEST_F(RestPathMatcher, find_case_insensitive)
{
    assertPathRegistered("/account/{accountId}/systems", 1);
    assertPathMatches("/accOUNT/vpupkIN/SYSTems", 1, { "vpupkIN" });
}

TEST_F(RestPathMatcher, no_suitable_handler)
{
    assertPathRegistered("/account/{accountId}/systems", 1);

    assertPathNotMatched("/account/vpupkin/info");
    assertPathNotMatched("/account/vpupkin/systems/");
    assertPathNotMatched("account/vpupkin/systems/");
    assertPathNotMatched("/account/systems");
}

TEST_F(RestPathMatcher, DISABLED_empty_parameter_value_is_not_accepted)
{
    assertPathRegistered("/account/{accountId}/systems", 1);
    assertPathNotMatched("/account//systems");
}

TEST_F(RestPathMatcher, registering_conflicting_handler)
{
    assertPathRegistered("/account/{accountId}/systems", 1);
    assertPathNotRegistered("/account/{accountId}/systems", 2);

    assertPathMatches("/account/vpupkin/systems", 1, {"vpupkin"});
}

} // namespace test
} // namespace rest
} // namespace server
} // namespace nx_http
