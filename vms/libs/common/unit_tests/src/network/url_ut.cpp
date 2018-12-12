#include <gtest/gtest.h>
#include <nx/utils/url.h>
#include <nx/utils/log/log_message.h>

#include <optional>

namespace nx {
namespace utils {
namespace test {

using namespace nx::utils;
static const QString kTestUrl = "http://user:passwd@[fe80::5be3:f02d:21e7:2450%3]:20431/some/path";
static const QString kTestIp = "fe80::5be3:f02d:21e7:2450%3";

static void checkUrl(const QString& urlString, const QString& hostString)
{
    auto nxUrl = Url(urlString);
    ASSERT_TRUE(nxUrl.isValid());
    ASSERT_EQ(hostString, nxUrl.host());
    ASSERT_EQ(urlString, nxUrl.toString());

    nxUrl.setHost("localhost");
    ASSERT_EQ("localhost", nxUrl.host());
    nxUrl.setHost(hostString);
    ASSERT_EQ(hostString, nxUrl.host());
    ASSERT_EQ(urlString, nxUrl.toString());
}

static void assertUrl(Url url, const QString& scheme, const QString& hostname, std::optional<int> port)
{
    ASSERT_EQ(url.scheme(), scheme);
    ASSERT_EQ(url.host(), hostname);
    if (port.has_value())
        ASSERT_EQ(url.port(), *port);
}

TEST(Url, ipv6_withScopeId)
{
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%3]:20431/some/path", kTestIp);
    checkUrl("http://user:passwd@[fe80::5be3:f02d:21e7:2450%3]:20431/some/path", kTestIp);
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%3]/some/path", kTestIp);
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%3]:20431", kTestIp);
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%3]", kTestIp);
}

TEST(Url, url_encoded)
{
    Url nxUrl(kTestUrl);
    ASSERT_EQ(kTestUrl, nxUrl.url());
    ASSERT_EQ(kTestUrl.toLatin1(), nxUrl.toEncoded());
}

TEST(Url, operator_less)
{
    const QString kTestUrl2 = "http://user:passwd@[fe80::5be3:f02d:21e7:2450%4]:20431/some/path";
    const QString kTestUrl3 = "http://user:passwd@[fe80::5be3:f02d:21e7:2450]:20431/some/path";

    ASSERT_LT(Url(kTestUrl), Url(kTestUrl2));
    ASSERT_LT(Url(kTestUrl3), Url(kTestUrl2));
    ASSERT_LT(Url(kTestUrl3), Url(kTestUrl));
}

TEST(Url, parseUrlFields)
{
    assertUrl(url::parseUrlFields("hostname"), "", "hostname", std::nullopt);
    assertUrl(url::parseUrlFields("hostname:1248"), "", "hostname", 1248);
    assertUrl(url::parseUrlFields("http://hostname"), "http", "hostname", std::nullopt);
    assertUrl(url::parseUrlFields("http://hostname:1248"), "http", "hostname", 1248);

    assertUrl(url::parseUrlFields("hostname", "zorz"), "zorz", "hostname", std::nullopt);
    assertUrl(url::parseUrlFields("hostname:1248", "zorz"), "zorz", "hostname", 1248);
    assertUrl(url::parseUrlFields("http://hostname", "zorz"), "http", "hostname", std::nullopt);
    assertUrl(url::parseUrlFields("http://hostname:1248", "zorz"), "http", "hostname", 1248);
}

TEST(Url, logging)
{
    Url url;
    if (nx::utils::ini().displayUrlPasswordInLogs == 0)
    {
        url.setScheme("http");
        url.setHost("zorz.com");
        url.setUserName("zorzuser");
        ASSERT_EQ(nx::utils::log::Message("%1").arg(url).toStdString(), "http://zorzuser@zorz.com");

        url.setPassword("zorzpassword");
        ASSERT_EQ(nx::utils::log::Message("%1").arg(url).toStdString(), "http://zorzuser@zorz.com");
    }
    else
    {
        url.clear();
        url.setScheme("http");
        url.setHost("zorz.com");
        url.setUserName("zorzuser");
        ASSERT_EQ(nx::utils::log::Message("%1").arg(url).toStdString(), "http://zorzuser@zorz.com");

        url.setPassword("zorzpassword");
        ASSERT_EQ(nx::utils::log::Message("%1").arg(url).toStdString(),
            "http://zorzuser:zorzpassword@zorz.com");
    }
}


} // namespace test
} // namespace utils
} // namespace nx
