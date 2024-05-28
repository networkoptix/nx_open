// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/auth_restriction_list.h>

namespace nx::network::http::test {

class AuthMethodRestrictionList:
    public ::testing::Test
{
public:
    AuthMethodRestrictionList():
        m_restrictionList(AuthMethod::notDefined)
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
            static_cast<AuthMethod>(1));

        m_restrictionList.allow(
            {std::nullopt, std::nullopt, "/customer/address/.*"},
            static_cast<AuthMethod>(2));

        m_restrictionList.allow(
            {"HTTP", Method::options, ".*"},
            static_cast<AuthMethod>(4));

        m_restrictionList.allow(
            {"RTMP", std::nullopt, ".*"},
            static_cast<AuthMethod>(8));
    }

    http::Request prepareRequest(
        const http::AuthMethodRestrictionList::Filter& filter)
    {
        http::Request request;
        request.requestLine.method = *filter.method;
        request.requestLine.version.protocol = *filter.protocol;
        request.requestLine.url.setPath(*filter.path);
        return request;
    }
};

TEST_F(AuthMethodRestrictionList, allow_by_path)
{
    assertMatchedTo({"HTTP", Method::get, "/customer/address"}, 1);
    assertMatchedTo({"HTTP", Method::get, "/customer/address/city"}, 2);
}

TEST_F(AuthMethodRestrictionList, allow_by_method_and_protocol)
{
    assertMatchedTo({"HTTP", Method::options, "/products/"}, 4);
    assertNotMatched({"RTSP", Method::options, "/factory/1/entrance"});
}

TEST_F(AuthMethodRestrictionList, matching_multiple_rules_simultaneously)
{
    assertMatchedTo({"HTTP", Method::options, "/customer/address"}, 4 | 1);
    assertMatchedTo({"RTMP", Method::get, "/customer/address"}, 8 | 1);
}

} // namespace nx::network::http::test
