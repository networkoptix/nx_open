#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>

namespace nx {
namespace network {
namespace url {
namespace test {

class UrlBuilderTest: public ::testing::Test
{
public:
    static void testHost(const QString& host)
    {
        const SocketAddress address(host, 7001);
        const auto url = nx::network::url::Builder()
            .setScheme(nx::network::http::urlSheme(true))
            .setEndpoint(address).toUrl();
        ASSERT_TRUE(url.isValid());
        ASSERT_FALSE(url.host().isEmpty());
    }
};

TEST_F(UrlBuilderTest, buildIpV6UrlFull)
{
    testHost("2001:0db8:11a3:09d7:1f34:8a2e:07a0:765d");
    testHost("[2001:0db8:11a3:09d7:1f34:8a2e:07a0:765d]");
}

TEST_F(UrlBuilderTest, buildIpV6UrlCollapsed)
{
    testHost("[2001::9d38:6ab8:384e:1c3e:aaa2]");
    testHost("2001::9d38:6ab8:384e:1c3e:aaa2");
    testHost("::");
    testHost("::1");
    testHost("2001::");
}

TEST_F(UrlBuilderTest, buildIpV6UrlWithZeroes)
{
    testHost("[2001:0000:11a3:09d7:1f34:8a2e:07a0:765d]");
    testHost("2001:0000:11a3:09d7:1f34:8a2e:07a0:765d");
}

TEST_F(UrlBuilderTest, buildIpV6UrlWithSingleZero)
{
    testHost("[2001:0:11a3:09d7:1f34:8a2e:07a0:765d]");
    testHost("2001:0:11a3:09d7:1f34:8a2e:07a0:765d");
}


} // namespace test
} // namespace url
} // namespace network
} // namespace nx
