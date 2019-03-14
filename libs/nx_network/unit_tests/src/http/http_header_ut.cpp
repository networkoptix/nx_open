#include <gtest/gtest.h>

#include <nx/network/http/http_types.h>
#include <nx/utils/random.h>

namespace nx {
namespace network {
namespace http {
namespace header {
namespace test {

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

TEST(HttpHeaderXForwardedFor, client_host_only_present)
{
    const char* testStr = "client.com";

    XForwardedFor xForwardedFor;
    ASSERT_TRUE(xForwardedFor.parse(testStr));

    ASSERT_EQ("client.com", xForwardedFor.client);
    ASSERT_TRUE(xForwardedFor.proxies.empty());
}

TEST(HttpHeaderXForwardedFor, proxy_hosts_present)
{
    const char* testStr = "client.com, proxy1.com, proxy2.com";

    XForwardedFor xForwardedFor;
    ASSERT_TRUE(xForwardedFor.parse(testStr));

    ASSERT_EQ("client.com", xForwardedFor.client);
    ASSERT_EQ(2U, xForwardedFor.proxies.size());
    ASSERT_EQ("proxy1.com", xForwardedFor.proxies[0]);
    ASSERT_EQ("proxy2.com", xForwardedFor.proxies[1]);
}

//-------------------------------------------------------------------------------------------------

class HttpHeaderForwarded:
    public ::testing::Test
{
protected:
    void assertParsedTo(
        const Forwarded& expected,
        const char* serializedActual)
    {
        Forwarded actual;
        ASSERT_TRUE(actual.parse(serializedActual));
        ASSERT_EQ(expected, actual);
    }

    void assertParseFails(const char* str)
    {
        Forwarded actual;
        ASSERT_FALSE(actual.parse(str));
    }
};

TEST_F(HttpHeaderForwarded, parse)
{
    assertParsedTo(
        {{{ "", "_gazonk", "", "" }}},
        "for=\"_gazonk\"");

    assertParsedTo(
        {{{ "", "[2001:db8:cafe::17]:4711", "", "" }}},
        "For=\"[2001:db8:cafe::17]:4711\"");

    assertParsedTo(
        {{{ "203.0.113.43", "192.0.2.60", "", "http" }}},
        "for=192.0.2.60;proto=http;by=203.0.113.43");

    assertParsedTo(
        {{{ "", "192.0.2.43", "", "" }, { "", "198.51.100.17", "", "" }}},
        "for=192.0.2.43, for=198.51.100.17");

    assertParseFails("");
    assertParseFails(",");
}

} // namespace test
} // namespace header
} // namespace nx
} // namespace network
} // namespace http
