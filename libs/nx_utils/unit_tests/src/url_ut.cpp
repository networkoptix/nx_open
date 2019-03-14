#include <gtest/gtest.h>

#include <optional>

#include <nx/utils/log/log.h>
#include <nx/utils/url.h>

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
    ASSERT_EQ(url::parseUrlFields("hostname").toStdString(), "//hostname");
    ASSERT_EQ(url::parseUrlFields("hostname:1248").toStdString(), "//hostname:1248");
    ASSERT_EQ(url::parseUrlFields("user:pass@hostname:1248").toStdString(),
        "//user:pass@hostname:1248");
    ASSERT_EQ(url::parseUrlFields("http://hostname").toStdString(), "http://hostname");
    ASSERT_EQ(url::parseUrlFields("http://hostname:1248").toStdString(), "http://hostname:1248");
    ASSERT_EQ(url::parseUrlFields("http://user:pass@hostname:1248/path?query").toStdString(),
        "http://user:pass@hostname:1248/path?query");

    ASSERT_EQ(url::parseUrlFields("hostname", "zorz").toStdString(), "zorz://hostname");
    ASSERT_EQ(url::parseUrlFields("user:pass@hostname:1248", "zorz").toStdString(),
        "zorz://user:pass@hostname:1248");
    ASSERT_EQ(url::parseUrlFields("http://hostname:1248", "zorz").toStdString(),
        "http://hostname:1248");

    ASSERT_EQ(url::parseUrlFields("invalid+hostname").path(), "invalid+hostname");
    ASSERT_FALSE(url::parseUrlFields("http://invalid+hostname:666").isValid());
    ASSERT_FALSE(url::parseUrlFields("h*ttp://60/path").isValid());
}

TEST(Url, toWebClientStandardViolatingUrl)
{
    ASSERT_EQ(Url("http://hostname/path?query#fragment").toWebClientStandardViolatingUrl(),
        "http://hostname/path#fragment?query");

    ASSERT_EQ(Url("http://hostname/path#fragment").toWebClientStandardViolatingUrl(),
        "http://hostname/path#fragment");

    ASSERT_EQ(Url("http://hostname/path?query").toWebClientStandardViolatingUrl(),
        "http://hostname/path?query");
}

TEST(Url, logging)
{
    Url url;
    if (!nx::utils::log::showPasswords())
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
