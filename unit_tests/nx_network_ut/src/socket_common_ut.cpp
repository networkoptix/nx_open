#ifdef _WIN32
#include <Winsock2.h>
typedef ULONG in_addr_t;
#endif

#include <gtest/gtest.h>

#include <nx/network/socket_common.h>

namespace {

static void testHostAddress(
    HostAddress host,
    boost::optional<in_addr_t> ipv4,
    boost::optional<in6_addr> ipv6,
    const char* string,
    bool isIpAddress)
{
    ASSERT_EQ(isIpAddress, host.isIpAddress());
    ASSERT_EQ(QString(string), host.toString());

    if (ipv4)
    {
        const auto ip =  host.ipV4();
        ASSERT_TRUE((bool)ip);
        ASSERT_EQ(ip->s_addr, *ipv4);
    }
    else
    {
        ASSERT_FALSE((bool)host.ipV4());
    }

    if (ipv6)
    {
        const auto ip =  host.ipV6().first;
        ASSERT_TRUE((bool)ip);
        ASSERT_EQ(0, memcmp(&ip.get(), &ipv6.get(), sizeof(in6_addr)));
    }
    else
    {
        ASSERT_FALSE((bool)host.ipV6().first);
    }
}

static void testHostAddress(
    const char* string4,
    const char* string6,
    boost::optional<in_addr_t> ipv4,
    boost::optional<in6_addr> ipv6)
{
    if (string4)
        testHostAddress(string4, ipv4, ipv6, string4, false);

    testHostAddress(string6, ipv4, ipv6, string6, false);

    if (ipv4)
    {
        in_addr addr;

        memset(&addr, 0, sizeof(addr));
        addr.s_addr = *ipv4;
        testHostAddress(addr, ipv4, ipv6, string4, true);
    }

    if (ipv6)
        testHostAddress(*ipv6, ipv4, ipv6, ipv4 ? string4 : string6, true);
}

} // namespace

TEST(HostAddress, Base)
{
    testHostAddress("0.0.0.0", "::", htonl(INADDR_ANY), in6addr_any);
    testHostAddress("127.0.0.1", "::1", htonl(INADDR_LOOPBACK), in6addr_loopback);

    const auto kIpV4a = HostAddress::ipV4from(QString("12.34.56.78"));
    const auto kIpV6a = HostAddress::ipV6from(QString("::ffff:c22:384e")).first;
    const auto kIpV6b = HostAddress::ipV6from(QString("2001:db8:0:2::1")).first;

    ASSERT_TRUE((bool)kIpV4a);
    ASSERT_TRUE((bool)kIpV6a);
    ASSERT_TRUE((bool)kIpV6b);

    testHostAddress("12.34.56.78", "::ffff:12.34.56.78", kIpV4a->s_addr, kIpV6a);
    testHostAddress(nullptr, "2001:db8:0:2::1", boost::none, kIpV6b);
}

TEST(HostAddress, IsLocal)
{
    ASSERT_TRUE(HostAddress("127.0.0.1").isLocal());
    ASSERT_TRUE(HostAddress("10.0.2.103").isLocal());
    ASSERT_TRUE(HostAddress("172.17.0.2").isLocal());
    ASSERT_TRUE(HostAddress("192.168.1.1").isLocal());
    ASSERT_TRUE(HostAddress("fd00::9465:d2ff:fe64:2772").isLocal());
    ASSERT_TRUE(HostAddress("fe80::d250:99ff:fe39:1d29").isLocal());
    ASSERT_TRUE(HostAddress("::ffff:172.25.4.8").isLocal());

    ASSERT_FALSE(HostAddress("12.34.56.78").isLocal());
    ASSERT_FALSE(HostAddress("95.31.136.2").isLocal());
    ASSERT_FALSE(HostAddress("172.8.0.2").isLocal());
    ASSERT_FALSE(HostAddress("2001:db8:0:2::1").isLocal());
    ASSERT_FALSE(HostAddress("::ffff:12.34.56.78").isLocal());
}

#if 0 // TODO: #ak CLOUD-1124

TEST(HostAddress, string_that_is_valid_ipv4)
{
    HostAddress hostAddress("127.0.0.1");
    ASSERT_EQ(HostAddress::localhost, hostAddress);
    // ipv4 string cannot be converted to ipv6 address, but MUST be resolved.
    ASSERT_FALSE(hostAddress.ipV6());
    ASSERT_TRUE(hostAddress.ipV4());
    ASSERT_EQ(htonl(INADDR_LOOPBACK), hostAddress.ipV4()->s_addr);
}

TEST(HostAddress, isIpAddress)
{
    HostAddress hostAddress("127.0.0.1");
    ASSERT_EQ(
        hostAddress.isIpAddress(),
        hostAddress.ipV4() || hostAddress.ipV6());
}

#endif

//-------------------------------------------------------------------------------------------------

namespace {

static void testSocketAddress(const char* init, const char* host, int port)
{
    const auto addr = SocketAddress(QString(init));
    ASSERT_EQ(addr.address.toString(), QString(host));
    ASSERT_EQ(addr.port, port);
    ASSERT_EQ(addr.toString(), QString(init));

    SocketAddress other(HostAddress(host), port);
    ASSERT_EQ(addr.toString(), other.toString());
    ASSERT_EQ(addr, other);

    QUrl url(QString("http://%1/path").arg(init));
    ASSERT_EQ(addr.address.toString(), url.host());
    ASSERT_EQ(addr.port, url.port(0));
}

} // namespace

TEST(SocketAddress, Base)
{
    testSocketAddress("ya.ru", "ya.ru", 0);
    testSocketAddress("ya.ru:80", "ya.ru", 80);

    testSocketAddress("12.34.56.78", "12.34.56.78", 0);
    testSocketAddress("12.34.56.78:123", "12.34.56.78", 123);

    testSocketAddress("[2001:db8:0:2::1]", "2001:db8:0:2::1", 0);
    testSocketAddress("[2001:db8:0:2::1]:23", "2001:db8:0:2::1", 23);

    testSocketAddress("[::ffff:12.34.56.78]", "::ffff:12.34.56.78", 0);
    testSocketAddress("[::ffff:12.34.56.78]:777", "::ffff:12.34.56.78", 777);
}
