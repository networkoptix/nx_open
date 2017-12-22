#include <gtest/gtest.h>

#include <nx/network/http/http_types.h>
#include <nx/utils/random.h>

namespace nx_http {
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

} // namespace test
} // namespace header
} // namespace nx_http
