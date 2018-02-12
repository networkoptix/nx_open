#include <gtest/gtest.h>

#include <nx/network/url/url_parse_helper.h>

namespace nx {
namespace network {
namespace url {
namespace test {

TEST(UrlGetEndpoint, defaultPortCorrespondsToUrlScheme)
{
    ASSERT_EQ(80, getEndpoint(QUrl("http://host/path")).port);
    ASSERT_EQ(443, getEndpoint(QUrl("https://host/path")).port);
    ASSERT_EQ(554, getEndpoint(QUrl("rtsp://host/path")).port);
    ASSERT_EQ(0, getEndpoint(QUrl("/host/path")).port);
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

} // namespace test
} // namespace url
} // namespace network
} // namespace nx
