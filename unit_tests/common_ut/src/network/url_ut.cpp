#include <gtest/gtest.h>
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

} // namespace test
} // namespace utils
} // namespace nx
