#include <gtest/gtest.h>

#include <nx/network/http/auth_restriction_list.h>

namespace nx::network::http::test {

class AuthMethodRestrictionList:
    public ::testing::Test
{
public:
    AuthMethodRestrictionList():
        m_restrictionList(0)
    {
    }

protected:
    int match(const http::AuthMethodRestrictionList::Filter& filter)
    {
        return static_cast<int>(
            m_restrictionList.getAllowedAuthMethods(prepareRequest(filter)));
    }

    void assertMatchedTo(
        const http::AuthMethodRestrictionList::Filter& filter,
        int expected)
    {
        ASSERT_EQ(expected, match(filter));
    }

    void assertNotMatched(
        const http::AuthMethodRestrictionList::Filter& filter)
    {
        ASSERT_EQ(0, match(filter));
    }

private:
    http::AuthMethodRestrictionList m_restrictionList;

    virtual void SetUp() override
    {
        m_restrictionList.allow(
            {std::nullopt, std::nullopt, "/customer/address"},
            static_cast<AuthMethod::Values>(1));

        m_restrictionList.allow(
            {std::nullopt, std::nullopt, "/customer/address/.*"},
            static_cast<AuthMethod::Values>(2));

        m_restrictionList.allow(
            {"HTTP", "OPTIONS", ".*"},
            static_cast<AuthMethod::Values>(4));

        m_restrictionList.allow(
            {"RTMP", std::nullopt, ".*"},
            static_cast<AuthMethod::Values>(8));
    }

    http::Request prepareRequest(
        const http::AuthMethodRestrictionList::Filter& filter)
    {
        http::Request request;
        request.requestLine.method = filter.method->c_str();
        request.requestLine.version.protocol = filter.protocol->c_str();
        request.requestLine.url.setPath(filter.path->c_str());
        return request;
    }
};

TEST_F(AuthMethodRestrictionList, allow_by_path)
{
    assertMatchedTo({"HTTP", "GET", "/customer/address"}, 1);
    assertMatchedTo({"HTTP", "GET", "/customer/address/city"}, 2);
}

TEST_F(AuthMethodRestrictionList, allow_by_method_and_protocol)
{
    assertMatchedTo({"HTTP", "OPTIONS", "/products/"}, 4);
    assertNotMatched({"RTSP", "OPTIONS", "/factory/1/entrance"});
}

TEST_F(AuthMethodRestrictionList, matching_multiple_rules_simultaneously)
{
    assertMatchedTo({"HTTP", "OPTIONS", "/customer/address"}, 4 | 1);
    assertMatchedTo({"RTMP", "GET", "/customer/address"}, 8 | 1);
}

} // namespace nx::network::http::test
