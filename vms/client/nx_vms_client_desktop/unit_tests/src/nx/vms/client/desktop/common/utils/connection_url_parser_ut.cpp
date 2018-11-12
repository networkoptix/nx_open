#include <gtest/gtest.h>

#include <nx/vms/client/desktop/common/utils/connection_url_parser.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

void assertParsedCorrectly(const QString& input, const std::string& expectedOutput)
{
    const auto url = helpers::parseConnectionUrlFromUserInput(input);
    ASSERT_EQ(url.toString().toStdString(), expectedOutput);
}

} // namespace

TEST(parseConnectionUrlFromUserInput, localhost)
{
    assertParsedCorrectly("localhost", "https://localhost:7001");
}

TEST(parseConnectionUrlFromUserInput, stripQuery)
{
    assertParsedCorrectly("https://localhost/test", "https://localhost:7001");
}

TEST(parseConnectionUrlFromUserInput, stripParams)
{
    assertParsedCorrectly("https://localhost?param=1", "https://localhost:7001");
}

TEST(parseConnectionUrlFromUserInput, stripAnchor)
{
    assertParsedCorrectly("https://localhost#anchor", "https://localhost:7001");
}

TEST(parseConnectionUrlFromUserInput, stripUsername)
{
    assertParsedCorrectly("https://user@localhost", "https://localhost:7001");
}

TEST(parseConnectionUrlFromUserInput, stripCredentials)
{
    assertParsedCorrectly("https://user:password@localhost", "https://localhost:7001");
}

TEST(parseConnectionUrlFromUserInput, stripEverything)
{
    assertParsedCorrectly("http://user:password@localhost:7002?p1=test&p2=&p3=a#anchor",
        "http://localhost:7002");
}

TEST(parseConnectionUrlFromUserInput, keepHttp)
{
    assertParsedCorrectly("http://127.0.0.1", "http://127.0.0.1:7001");
}

TEST(parseConnectionUrlFromUserInput, fixInvalidProtocol)
{
    assertParsedCorrectly("htt://127.0.0.1", "https://127.0.0.1:7001");
}

TEST(parseConnectionUrlFromUserInput, parseIpv6WithoutBraces)
{
    assertParsedCorrectly("fe80::a4b8:7e4e:7ebe:8277",
        "https://[fe80::a4b8:7e4e:7ebe:8277]:7001");
}

TEST(parseConnectionUrlFromUserInput, parseIpv6WithBraces)
{
    assertParsedCorrectly("[fe80::a4b8:7e4e:7ebe:8277]",
        "https://[fe80::a4b8:7e4e:7ebe:8277]:7001");
}

TEST(parseConnectionUrlFromUserInput, parseIpv6WithBracesAndPort)
{
    assertParsedCorrectly("[fe80::a4b8:7e4e:7ebe:8277]:7002",
        "https://[fe80::a4b8:7e4e:7ebe:8277]:7002");
}

TEST(parseConnectionUrlFromUserInput, parseIpv6WithProtocolBracesAndPort)
{
    assertParsedCorrectly("https://[fe80::a4b8:7e4e:7ebe:8277]:7002",
        "https://[fe80::a4b8:7e4e:7ebe:8277]:7002");
}
TEST(parseConnectionUrlFromUserInput, parseScopedIpv6WithoutBraces)
{
    assertParsedCorrectly("fe80::a4b8:7e4e:7ebe:8277%11",
        "https://[fe80::a4b8:7e4e:7ebe:8277%11]:7001");
}

TEST(parseConnectionUrlFromUserInput, parseScopedIpv6WithBraces)
{
    assertParsedCorrectly("[fe80::a4b8:7e4e:7ebe:8277%11]",
        "https://[fe80::a4b8:7e4e:7ebe:8277%11]:7001");
}

TEST(parseConnectionUrlFromUserInput, parseScopedIpv6WithBracesAndPort)
{
    assertParsedCorrectly("[fe80::a4b8:7e4e:7ebe:8277%11]:7002",
        "https://[fe80::a4b8:7e4e:7ebe:8277%11]:7002");
}

TEST(parseConnectionUrlFromUserInput, parseScopedIpv6WithProtocolBracesAndPort)
{
    assertParsedCorrectly("https://[fe80::a4b8:7e4e:7ebe:8277%11]:7002",
        "https://[fe80::a4b8:7e4e:7ebe:8277%11]:7002");
}

} // namespace test
} // namespace nx::vms::client::desktop
