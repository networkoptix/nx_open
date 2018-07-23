#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>

namespace nx {
namespace network {
namespace url {
namespace test {

TEST(UrlBuilder, buildIpV6Url)
{
    const SocketAddress address("2001::9d38:6ab8:384e:1c3e:aaa2", 7001);

    const auto url = nx::network::url::Builder()
            .setScheme(nx::network::http::urlSheme(true))
            .setEndpoint(address).toUrl();
    ASSERT_FALSE(url.host().isEmpty());
    ASSERT_TRUE(url.isValid());
}

} // namespace test
} // namespace url
} // namespace network
} // namespace nx
