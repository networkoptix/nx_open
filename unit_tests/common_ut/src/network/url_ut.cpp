#include <gtest/gtest.h>
#include <nx/utils/url.h>

namespace nx {
namespace utils {
namespace test {

using namespace nx::utils;

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
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%3]:20431/some/path", "fe80::5be3:f02d:21e7:2450%3");
    checkUrl("http://user:passwd@[fe80::5be3:f02d:21e7:2450%3]:20431/some/path", "fe80::5be3:f02d:21e7:2450%3");
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%3]/some/path", "fe80::5be3:f02d:21e7:2450%3");
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%3]:20431", "fe80::5be3:f02d:21e7:2450%3");
    checkUrl("http://[fe80::5be3:f02d:21e7:2450%3]", "fe80::5be3:f02d:21e7:2450%3");
}

} // namespace test
} // namespace utils
} // namespace nx
