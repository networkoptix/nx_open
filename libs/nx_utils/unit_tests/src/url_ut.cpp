// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <optional>

#include <nx/utils/log/log.h>
#include <nx/utils/url.h>

namespace nx {
namespace utils {
namespace test {

using namespace nx::utils;
static const QString kTestUrl = "http://user:passwd@[fe80::5be3:f02d:21e7:2450%253]:20431/some/path";
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
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%253]:20431/some/path", kTestIp);
    checkUrl("http://user:passwd@[fe80::5be3:f02d:21e7:2450%253]:20431/some/path", kTestIp);
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%253]/some/path", kTestIp);
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%253]:20431", kTestIp);
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%253]", kTestIp);
}

TEST(Url, setHost)
{
    Url url;

    url.setHost("zorz");
    ASSERT_EQ(url.host(), "zorz");
    url.setHost("");
    ASSERT_EQ(url.host(), "");

    url.setHost("zorz-123");
    ASSERT_EQ(url.host(), "zorz-123");
    url.setHost("invalid_ho$tname_%");
    ASSERT_EQ(url.host(), QString());
    url.setHost(QString());
    ASSERT_EQ(url.host(), QString());

}

TEST(Url, url_encoded)
{
    Url nxUrl(kTestUrl);
    ASSERT_EQ(kTestUrl, nxUrl.url());
    ASSERT_EQ(kTestUrl.toLatin1(), nxUrl.toEncoded());
}

TEST(Url, operator_less)
{
    const QString kTestUrl2 = "http://user:passwd@[fe80::5be3:f02d:21e7:2450%254]:20431/some/path";
    const QString kTestUrl3 = "http://user:passwd@[fe80::5be3:f02d:21e7:2450]:20431/some/path";

    ASSERT_LT(Url(kTestUrl), Url(kTestUrl2));
    ASSERT_LT(Url(kTestUrl2), Url(kTestUrl3));
    ASSERT_LT(Url(kTestUrl), Url(kTestUrl3));
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

    ASSERT_TRUE(url::parseUrlFields("abcd.efg-hik.net/").isValid());
    ASSERT_EQ(url::parseUrlFields("abcd.efg-hik.net/").host(), "abcd.efg-hik.net");
    ASSERT_TRUE(url::parseUrlFields("abcd.efg-hik.net/").path().isEmpty());

    ASSERT_TRUE(url::parseUrlFields("http://1.2.3.4/path1/").isValid());
    ASSERT_EQ(url::parseUrlFields("http://1.2.3.4/path1/").host(), "1.2.3.4");
    ASSERT_EQ(url::parseUrlFields("http://1.2.3.4/path1/").path(), "/path1/");
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
        url.setHost("example.com");
        url.setUserName("johndoe");
        ASSERT_EQ(nx::format("%1", url).toStdString(), "http://johndoe@example.com");

        url.setPassword("sample_password");
        ASSERT_EQ(nx::format("%1", url).toStdString(), "http://johndoe@example.com");
    }
    else
    {
        url.clear();
        url.setScheme("http");
        url.setHost("example.com");
        url.setUserName("johndoe");
        ASSERT_EQ(nx::format("%1", url).toStdString(), "http://johndoe@example.com");

        url.setPassword("sample_password");
        ASSERT_EQ(nx::format("%1", url).toStdString(), "http://johndoe:sample_password@example.com");
    }
}

TEST(Url, fromUserInput)
{
    const auto expectEq =
        [](const QString& input, const QString& expectedUrl)
        {
            EXPECT_EQ(expectedUrl, Url::fromUserInput(input).toString());
        };

    expectEq("192.168.0.1", "http://192.168.0.1");
    expectEq("192.168.0.2:7001", "http://192.168.0.2:7001");
    expectEq("192.168.0.3:7001/api/call", "http://192.168.0.3:7001/api/call");

    expectEq("example.com", "http://example.com");
    expectEq("example.com:8080", "http://example.com:8080");
    expectEq("example.com/index.html", "http://example.com/index.html");

    expectEq("abcd.efg-hik.net/", "http://abcd.efg-hik.net/");
    ASSERT_TRUE(Url::fromUserInput("abcd.efg-hik.net/").isValid());
    ASSERT_EQ(Url::fromUserInput("abcd.efg-hik.net/").host(), "abcd.efg-hik.net");
    ASSERT_EQ(Url::fromUserInput("abcd.efg-hik.net/").path(), "/");
}

TEST(Url, hidePassword)
{
    nx::kit::IniConfig::Tweaks iniTweaks;
    iniTweaks.set(&nx::utils::ini().showPasswordsInLogs, false);

    EXPECT_EQ(
        url::hidePassword("smb://johndoe:qweasd123@example.com/johndoe"),
        "smb://johndoe@example.com/johndoe");

    EXPECT_EQ(
        url::hidePassword("/api/storageStatus?path=smb://TestUser:TestPassword@192.168.1.40/sh1"),
        "/api/storageStatus?path=smb://TestUser@192.168.1.40/sh1");

    EXPECT_EQ(
        url::hidePassword("/api/storageStatus?path=smb%3A%2F%2FTestUser%3ATestPassword%40192.168.1.40%2Fsh1"),
        "/api/storageStatus?path=smb://TestUser@192.168.1.40/sh1");
}

TEST(Url, hidePassword_debug)
{
    nx::kit::IniConfig::Tweaks iniTweaks;
    iniTweaks.set(&nx::utils::ini().showPasswordsInLogs, true);

    EXPECT_EQ(
        url::hidePassword("smb://johndoe:qweasd123@example.com/johndoe"),
        "smb://johndoe:qweasd123@example.com/johndoe");

    EXPECT_EQ(
        url::hidePassword("/api/storageStatus?path=smb://TestUser:TestPassword@192.168.1.40/sh1"),
        "/api/storageStatus?path=smb://TestUser:TestPassword@192.168.1.40/sh1");

    EXPECT_EQ(
        url::hidePassword("/api/storageStatus?path=smb%3A%2F%2FTestUser%3ATestPassword%40192.168.1.40%2Fsh1"),
        "/api/storageStatus?path=smb%3A%2F%2FTestUser%3ATestPassword%40192.168.1.40%2Fsh1");
}

TEST(Url, webPageHostsEqual)
{
    EXPECT_TRUE(url::webPageHostsEqual("https://www.example.org/", "https://example.org/"));
    EXPECT_FALSE(url::webPageHostsEqual("https://www.example.com/", "https://ftp.example.com/"));
}

} // namespace test
} // namespace utils
} // namespace nx
