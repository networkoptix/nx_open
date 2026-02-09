// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/http_types.h>
#include <nx/network/utils.h>
#include <nx/ranges.h>
#include <nx/utils/random.h>

namespace nx::network::http::header::test {

namespace {

using nx::network::http::HttpHeaders;

class HttpHeaderStrictTransportSecurity:
    public ::testing::Test
{
protected:
    void givenHeaderWithRandomValues()
    {
        m_originalHeader.maxAge =
            std::chrono::seconds(nx::utils::random::number<int>(1, 100000));
        m_originalHeader.includeSubDomains = nx::utils::random::number<bool>();
        m_originalHeader.preload = nx::utils::random::number<bool>();

        m_serializedHeader = m_originalHeader.toString();
    }

    void andWithRandomAttribute()
    {
        m_serializedHeader += ";hope_they_will_not_add_this_string_to_rfc";
    }

    void whenParseSerializedHeaderRepresentation()
    {
        ASSERT_TRUE(m_parsedHeader.parse(m_serializedHeader));
    }

    void thenParsedHeaderMatchesOriginal()
    {
        ASSERT_EQ(m_originalHeader, m_parsedHeader);
    }

private:
    StrictTransportSecurity m_originalHeader;
    StrictTransportSecurity m_parsedHeader;
    nx::Buffer m_serializedHeader;
};

//-------------------------------------------------------------------------------------------------

TEST_F(HttpHeaderStrictTransportSecurity, serialize)
{
    StrictTransportSecurity header;
    header.maxAge = std::chrono::seconds(101);
    ASSERT_EQ("max-age=101", header.toString());

    header.includeSubDomains = true;
    ASSERT_EQ("max-age=101;includeSubDomains", header.toString());

    header.preload = true;
    ASSERT_EQ("max-age=101;includeSubDomains;preload", header.toString());
}

TEST_F(HttpHeaderStrictTransportSecurity, parse)
{
    givenHeaderWithRandomValues();
    whenParseSerializedHeaderRepresentation();
    thenParsedHeaderMatchesOriginal();
}

TEST_F(HttpHeaderStrictTransportSecurity, parse_ignores_unknown_attributes)
{
    givenHeaderWithRandomValues();
    andWithRandomAttribute();

    whenParseSerializedHeaderRepresentation();

    thenParsedHeaderMatchesOriginal();
}

//-------------------------------------------------------------------------------------------------

TEST(HttpHeaderXForwardedFor, parsing_with_client_host_only_present)
{
    const char* testStr = "client.com";

    XForwardedFor xForwardedFor;
    ASSERT_TRUE(xForwardedFor.parse(testStr));

    ASSERT_EQ("client.com", xForwardedFor.client);
    ASSERT_TRUE(xForwardedFor.proxies.empty());
}

TEST(HttpHeaderXForwardedFor, parsing_with_proxy_hosts_present)
{
    const char* testStr = "client.com, proxy1.com, proxy2.com";

    XForwardedFor xForwardedFor;
    ASSERT_TRUE(xForwardedFor.parse(testStr));

    ASSERT_EQ("client.com", xForwardedFor.client);
    ASSERT_EQ(2U, xForwardedFor.proxies.size());
    ASSERT_EQ("proxy1.com", xForwardedFor.proxies[0]);
    ASSERT_EQ("proxy2.com", xForwardedFor.proxies[1]);
}

TEST(HttpHeaderXForwardedFor, serialize)
{
    XForwardedFor xForwardedFor{"client.com", {{"proxy1.com"}, {"proxy2.com"}}};

    ASSERT_EQ("client.com, proxy1.com, proxy2.com", xForwardedFor.toString());
}

//-------------------------------------------------------------------------------------------------

class HttpHeaderForwarded:
    public ::testing::Test
{
protected:
    void assertParseAndSerializeAreSymmetric(
        const Forwarded& header,
        const char* serializedActual)
    {
        assertParsedTo(header, serializedActual);
        assertSerializedTo(serializedActual, header);
    }

    void assertParseFails(const char* str)
    {
        Forwarded actual;
        ASSERT_FALSE(actual.parse(str));
    }

    void assertParsedTo(
        const Forwarded& expected,
        const char* serializedActual)
    {
        Forwarded actual;
        ASSERT_TRUE(actual.parse(serializedActual));
        ASSERT_EQ(expected, actual);
    }

    void assertSerializedTo(
        const char* expected,
        const Forwarded& header)
    {
        ASSERT_EQ(
            nx::utils::toLower(std::string_view(expected)),
            nx::utils::toLower(header.toString()));
    }
};

TEST_F(HttpHeaderForwarded, parse)
{
    assertParsedTo(
        {{{ "", "_gazonk", "", "" }}},
        "for=\"_gazonk\"");

    assertParseFails("");
    assertParseFails(",");
}

TEST_F(HttpHeaderForwarded, parse_and_serialize_are_symmetric)
{
    assertParseAndSerializeAreSymmetric(
        {{{ "", "_gazonk", "", "" }}},
        "for=_gazonk");

    assertParseAndSerializeAreSymmetric(
        {{{ "", "[2001:db8:cafe::17]:4711", "", "" }}},
        "For=\"[2001:db8:cafe::17]:4711\"");

    assertParseAndSerializeAreSymmetric(
        {{{ "203.0.113.43", "192.0.2.60", "example.com:1234", "http" }}},
        "by=203.0.113.43;for=192.0.2.60;host=\"example.com:1234\";proto=http");

    assertParseAndSerializeAreSymmetric(
        {{{ "", "192.0.2.43", "", "" }, { "", "198.51.100.17", "", "" }}},
        "for=192.0.2.43, for=198.51.100.17");
}

using CookieNamePair = std::pair<std::string_view, std::string_view>;

static const std::pair<HttpHeaders, CookieNamePair> kCases[] = {
    {{{"Cookie", ""}}, {"", ""}},
    {{{"Cookie", "; "}}, {"", ""}},
    {{{"Cookie", "sid=abc"}}, {"sid", "abc"}},
    {{{"Cookie", "a=1; b=2"}}, {"a", "1"}},
    {{{"Cookie", "c=3"}, {"Cookie", "d=4"}, {"Other-Header", "v"}}, {"c", "3"}},
    {{{"Cookie", "   token=value  "}}, {"token", "value"}},
    {{{"Cookie", ";foo=bar"}}, {"foo", "bar"}},
    {{{"Cookie", "foo=bar; "}}, {"foo", "bar"}},
    {{{"Cookie", "key="}}, {"key", ""}},
    {{{"Cookie", "q=\"quoted value\"; x=y"}}, {"q", "\"quoted value\""}},
    {{{"Cookie", "   k = v "}, {"Cookie", "z=9"}}, {"k", "v"}},
    {{/* empty */}, {"", ""}},

    // This should be reported, but as of this writing are just ignored.
    {{{"Cookie", "invalid"}}, {"", ""}},
    {{{"Cookie", "invalid= also_invalid=;"}}, {"", ""}},
};

class CookieParseTest: public ::testing::TestWithParam<std::pair<HttpHeaders, CookieNamePair>>
{
};

TEST_P(CookieParseTest, cookiesView)
{
    const auto& [headers, expected] = GetParam();
    const std::vector parsed = headers
        | nx::network::utils::cookiesView()
        | nx::ranges::to<std::vector>();

    if (expected.first.empty() && expected.second.empty())
    {
        EXPECT_TRUE(parsed.empty());
        return;
    }

    ASSERT_FALSE(parsed.empty()) << "No cookies parsed";
    EXPECT_EQ(parsed.front().first, expected.first);
    EXPECT_EQ(parsed.front().second, expected.second);
}

INSTANTIATE_TEST_SUITE_P(Headers, CookieParseTest, ::testing::ValuesIn(kCases));

} // namespace
} // namespace nx::network::http::header::test
