#include <gtest/gtest.h>

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
    ASSERT_EQ(0, getEndpoint(nx::utils::Url("/host/path")).port);
}

} // namespace test
} // namespace url
} // namespace network
} // namespace nx
