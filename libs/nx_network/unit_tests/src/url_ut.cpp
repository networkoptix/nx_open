// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/network/url/url_parse_helper.h>

namespace nx {
namespace network {
namespace url {
namespace test {

TEST(UrlGetEndpoint, defaultPortCorrespondsToUrlScheme)
{
    ASSERT_EQ(80, getEndpoint(nx::utils::Url("http://host/path")).port);
    ASSERT_EQ(443, getEndpoint(nx::utils::Url("https://host/path")).port);
    ASSERT_EQ(554, getEndpoint(nx::utils::Url("rtsp://host/path")).port);
}

TEST(Url, joinPath)
{
    ASSERT_EQ("/path/somewhere", nx::network::url::joinPath("/path/", "/somewhere"));
    ASSERT_EQ("/path/somewhere", nx::network::url::joinPath("/path", "somewhere"));
    ASSERT_EQ("path/somewhere", nx::network::url::joinPath("path/", "/somewhere"));
    ASSERT_EQ("/some_path", nx::network::url::joinPath("/", "/some_path"));
    ASSERT_EQ("/some_path", nx::network::url::joinPath("/", "/some_path"));
    ASSERT_EQ("/some_path", nx::network::url::joinPath("", "/some_path"));
    ASSERT_EQ("/some_path", nx::network::url::joinPath("", "some_path"));
}

// NOTE: Copy-paste from url test in utils in order to check cross-library behavior.
TEST(Url, logging)
{
    nx::utils::Url url;
    if (!nx::log::showPasswords())
    {
        url.setScheme("http");
        url.setHost("zorz.com");
        url.setUserName("zorzuser");
        ASSERT_EQ(nx::format("%1", url).toStdString(), "http://zorzuser@zorz.com");

        url.setPassword("zorzpassword");
        ASSERT_EQ(nx::format("%1", url).toStdString(), "http://zorzuser@zorz.com");
    }
    else
    {
        url.clear();
        url.setScheme("http");
        url.setHost("zorz.com");
        url.setUserName("zorzuser");
        ASSERT_EQ(nx::format("%1", url).toStdString(), "http://zorzuser@zorz.com");

        url.setPassword("zorzpassword");
        ASSERT_EQ(nx::format("%1", url).toStdString(), "http://zorzuser:zorzpassword@zorz.com");
    }
}

TEST(Url, ipv6_host)
{
    static constexpr char kUrl[] = "http://[fe80::f80b:5eab:64a8:7b63%2514]:7001/foo";

    nx::utils::Url url(kUrl);
    ASSERT_TRUE(url.isValid());
    ASSERT_EQ("http", url.scheme());
    ASSERT_EQ("fe80::f80b:5eab:64a8:7b63%14", url.host());
    ASSERT_EQ(7001, url.port());
    ASSERT_EQ("/foo", url.path());

    ASSERT_EQ(kUrl, url.toString());
}

} // namespace test
} // namespace url
} // namespace network
} // namespace nx
