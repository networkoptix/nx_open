#include <gtest/gtest.h>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/nx_utils_ini.h>

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
} // namespace url
} // namespace network
} // namespace nx
