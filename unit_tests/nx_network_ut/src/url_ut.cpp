#include <gtest/gtest.h>

#include <nx/utils/log/log_message.h>
#include <nx/network/url/url_parse_helper.h>

namespace nx::network::url::test {

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

TEST(Url, toString)
{
    nx::utils::Url url;
    url.setScheme("http");
    url.setHost("zorz.com");
    ASSERT_EQ(nx::utils::log::Message("%1").arg(url).toStdString(), "http://zorz.com");

    url.setUserName("zorzuser");
    ASSERT_EQ(nx::utils::log::Message("%1").arg(url).toStdString(), "http://zorzuser@zorz.com");

    url.setPassword("zorzpassword"); //< Password must be omitted when logging.
    ASSERT_EQ(nx::utils::log::Message("%1").arg(url).toStdString(), "http://zorzuser@zorz.com");
}

} // namespace nx::network::url::test
