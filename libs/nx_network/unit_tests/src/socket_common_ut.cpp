// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2tcpip.h>
typedef ULONG in_addr_t;
#endif

#include <gtest/gtest.h>

#include <nx/network/socket_common.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/string.h>
#include <nx/utils/url.h>

namespace nx::network::test {

namespace {

static void testHostAddress(
    HostAddress host,
    std::optional<in_addr_t> ipv4,
    std::optional<in6_addr> ipv6,
    const char* string,
    bool isIpAddress)
{
    ASSERT_EQ(isIpAddress, host.isIpAddress());
    ASSERT_EQ(std::string(string), host.toString());

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
        ASSERT_EQ(ip, ipv6);
    }
    else
    {
        ASSERT_FALSE((bool)host.ipV6().first);
    }
}

static void testHostAddress(
    const char* string4,
    const char* string6,
    std::optional<in_addr_t> ipv4,
    std::optional<in6_addr> ipv6)
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

    const auto kIpV4a = HostAddress::ipV4from(std::string("12.34.56.78"));
    const auto kIpV6a = HostAddress::ipV6from(std::string("::ffff:c22:384e")).first;
    const auto kIpV6b = HostAddress::ipV6from(std::string("2001:db8:0:2::1")).first;

    ASSERT_TRUE((bool)kIpV4a);
    ASSERT_TRUE((bool)kIpV6a);
    ASSERT_TRUE((bool)kIpV6b);

    testHostAddress("12.34.56.78", "::ffff:12.34.56.78", kIpV4a->s_addr, kIpV6a);
    testHostAddress(nullptr, "2001:db8:0:2::1", std::nullopt, kIpV6b);
}

TEST(HostAddress, localhost)
{
    ASSERT_EQ(HostAddress::ipV6from("::1"), HostAddress::localhost.ipV6());
    ASSERT_EQ(in6addr_loopback, *HostAddress::localhost.ipV6().first);

    ASSERT_EQ(*HostAddress::ipV4from("127.0.0.1"), *HostAddress::localhost.ipV4());
    in_addr loopback;
    memset(&loopback, 0, sizeof(loopback));
    loopback.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(loopback, *HostAddress::localhost.ipV4());

    ASSERT_EQ("localhost", HostAddress::localhost.toString());
}

TEST(HostAddress, IpV6FromString)
{
    HostAddress addr("fd00::9465:d2ff:fe64:2772%1");
    ASSERT_FALSE(addr.isIpAddress());
    ASSERT_TRUE(addr.isPureIpV6());

    in6_addr addr6;
    ASSERT_TRUE(inet_pton(AF_INET6, "fd00::9465:d2ff:fe64:2772", &addr6));
    ASSERT_EQ(addr6, *addr.ipV6().first);
    ASSERT_EQ(1U, *addr.ipV6().second);
}

TEST(HostAddress, MappedIpV4AddressIsNotAPureIpV6)
{
    HostAddress addr("::ffff:172.25.4.8");
    ASSERT_FALSE(addr.isPureIpV6());

    addr = HostAddress("::ffff:c22:384e");
    ASSERT_FALSE(addr.isPureIpV6());
}

TEST(HostAddress, IpToStringV6)
{
    in6_addr addr6;
    ASSERT_TRUE(inet_pton(AF_INET6, "fd00::9465:d2ff:fe64:2772", &addr6));
    ASSERT_EQ("fd00::9465:d2ff:fe64:2772%1", *HostAddress::ipToString(addr6, 1));

    HostAddress addr(addr6, 1);
    ASSERT_EQ("fd00::9465:d2ff:fe64:2772%1", addr.toString());
}

TEST(HostAddress, IsLocalNetwork)
{
    EXPECT_TRUE(HostAddress("127.0.0.1").isLocalNetwork());
    EXPECT_TRUE(HostAddress("10.0.2.103").isLocalNetwork());
    EXPECT_TRUE(HostAddress("172.17.0.2").isLocalNetwork());
    EXPECT_TRUE(HostAddress("192.168.1.1").isLocalNetwork());
    EXPECT_TRUE(HostAddress("fd00::9465:d2ff:fe64:2772%1").isLocalNetwork());
    EXPECT_TRUE(HostAddress("fe80::d250:99ff:fe39:1d29%2").isLocalNetwork());
    EXPECT_TRUE(HostAddress("::ffff:172.25.4.8").isLocalNetwork());

    EXPECT_FALSE(HostAddress("12.34.56.78").isLocalNetwork());
    EXPECT_FALSE(HostAddress("95.31.136.2").isLocalNetwork());
    EXPECT_FALSE(HostAddress("172.8.0.2").isLocalNetwork());
    EXPECT_FALSE(HostAddress("2001:db8:0:2::1").isLocalNetwork());
    EXPECT_FALSE(HostAddress("::ffff:12.34.56.78").isLocalNetwork());
}

TEST(HostAddress, isLoopback_ipv4)
{
    ASSERT_TRUE(HostAddress("localhost").isLoopback());

    ASSERT_TRUE(HostAddress("127.0.0.0").isLoopback());
    ASSERT_TRUE(HostAddress("127.0.0.1").isLoopback());
    ASSERT_TRUE(HostAddress("127.255.255.255").isLoopback());
    ASSERT_TRUE(HostAddress(*HostAddress::ipV4from("127.0.0.1")).isLoopback());

    ASSERT_FALSE(HostAddress("128.0.0.0").isLoopback());
    ASSERT_FALSE(HostAddress("126.255.255.255").isLoopback());
    ASSERT_FALSE(HostAddress("10.1.5.1").isLoopback());
}

TEST(HostAddress, isLoopback_ipv6)
{
    ASSERT_TRUE(HostAddress("::1").isLoopback());
    ASSERT_TRUE(HostAddress(*HostAddress::ipV6from("::1").first).isLoopback());

    ASSERT_FALSE(HostAddress("::2").isLoopback());
    ASSERT_FALSE(HostAddress("::ffff:12.34.56.78").isLoopback());
}

TEST(HostAddress, isIpAddress)
{
    ASSERT_FALSE(HostAddress("127.0.0.1").isIpAddress());
}

TEST(HostAddress, modifications)
{
    HostAddress default_;
    ASSERT_EQ(HostAddress(default_.toString() + "_api").toString(), QString("0.0.0.0_api"));

    HostAddress empty("");
    ASSERT_EQ(HostAddress(empty.toString() + "_api").toString(), QString("_api"));

    HostAddress domain("example.com");
    ASSERT_EQ(HostAddress(domain.toString() + "_api").toString(), QString("example.com_api"));

    HostAddress ipv4("192.168.12.34");
    ASSERT_EQ(HostAddress(ipv4.toString() + "_api").toString(), QString("192.168.12.34_api"));

    HostAddress ipv6("::1");
    auto ipv6s = ipv6.toString() + "_api";
    ipv6s = nx::utils::replace(ipv6s, ":", "_");
    ASSERT_EQ(HostAddress(ipv6s).toString(), QString("__1_api"));
}

TEST(HostAddress, isMulticast_v4)
{
    ASSERT_TRUE(HostAddress("224.0.0.0").isMulticast());
    ASSERT_FALSE(HostAddress("223.255.255.255").isMulticast());

    ASSERT_TRUE(HostAddress("239.255.255.255").isMulticast());
    ASSERT_FALSE(HostAddress("240.0.0.0").isMulticast());

    ASSERT_FALSE(HostAddress("127.0.0.1").isMulticast());
    ASSERT_FALSE(HostAddress("localhost").isMulticast());
    ASSERT_FALSE(HostAddress("example.com").isMulticast());
}

TEST(HostAddress, isMulticast_v6)
{
    ASSERT_TRUE(HostAddress("ff00::1").isMulticast());
    ASSERT_FALSE(HostAddress("feff::1").isMulticast());

    ASSERT_TRUE(HostAddress("ffff::1").isMulticast());
}

#if 0 // TODO: #akolesnikov CLOUD-1124

TEST(HostAddress, string_that_is_valid_ipv4_is_not_converted_to_ip_implicitly)
{
    HostAddress hostAddress("127.0.0.1");

    ASSERT_TRUE(hostAddress.ipV4());
    ASSERT_EQ(htonl(INADDR_LOOPBACK), hostAddress.ipV4()->s_addr);

    // In IPv6-only network every ipv4 address string MUST be resolved.
    // So, it cannot be converted to an IP address implicitly.
    ASSERT_FALSE(hostAddress.isIpAddress());
    ASSERT_NE(HostAddress::localhost, hostAddress);
    ASSERT_FALSE(static_cast<bool>(hostAddress.ipV6().first));
}

#endif

TEST(HostAddress, compatible_with_nx_fusion)
{
    ASSERT_EQ("\"localhost\"", QJson::serialized(nx::network::HostAddress("localhost")));

    nx::network::HostAddress value;
    ASSERT_TRUE(QJson::deserialize(nx::Buffer("\"localhost\""), &value));
    ASSERT_EQ(nx::network::HostAddress("localhost"), value);
}

//-------------------------------------------------------------------------------------------------

class SocketAddress:
    public ::testing::Test
{
protected:
    void assertEndpointIsParsedAsExpected(
        const network::SocketAddress& endpoint,
        const std::string& expectedEndpointString,
        const char* host,
        int port)
    {
        ASSERT_EQ(host, endpoint.address.toString());
        ASSERT_EQ(port, endpoint.port);
        ASSERT_EQ(expectedEndpointString, endpoint.toString());

        network::SocketAddress other(HostAddress(host), port);
        ASSERT_EQ(endpoint.toString(), other.toString());
        ASSERT_EQ(endpoint, other);

        nx::utils::Url url(nx::format("http://%1/path").arg(expectedEndpointString));
        ASSERT_EQ(endpoint.address.toString(), url.host().toStdString());
        ASSERT_EQ(endpoint.port, url.port(0));
    }

    void assertEndpointIsParsedAsExpected(
        const std::string& endpointString, const char* host, int port)
    {
        assertEndpointIsParsedAsExpected(
            network::SocketAddress(endpointString),
            endpointString,
            host,
            port);
    }

    network::SocketAddress socketAddressFromUserInput(const QString& url)
    {
        return network::SocketAddress::fromUrl(nx::utils::Url::fromUserInput(url));
    }
};

TEST_F(SocketAddress, endpoint_with_hostname)
{
    assertEndpointIsParsedAsExpected("example.com", "example.com", 0);
    assertEndpointIsParsedAsExpected("example.com:80", "example.com", 80);
}

TEST_F(SocketAddress, endpoint_with_ipv4)
{
    assertEndpointIsParsedAsExpected("12.34.56.78", "12.34.56.78", 0);
    assertEndpointIsParsedAsExpected("12.34.56.78:123", "12.34.56.78", 123);
}

TEST_F(SocketAddress, endpoint_with_ipv6)
{
    assertEndpointIsParsedAsExpected(
        network::SocketAddress("2001:db8:0:2::1"),
        "[2001:db8:0:2::1]",
        "2001:db8:0:2::1",
        0);

    assertEndpointIsParsedAsExpected("[2001:db8:0:2::1]", "2001:db8:0:2::1", 0);
    assertEndpointIsParsedAsExpected("[2001:db8:0:2::1]:23", "2001:db8:0:2::1", 23);

    assertEndpointIsParsedAsExpected(
        network::SocketAddress("::ffff:12.34.56.78"),
        "[::ffff:12.34.56.78]",
        "::ffff:12.34.56.78",
        0);

    assertEndpointIsParsedAsExpected("[::ffff:12.34.56.78]", "::ffff:12.34.56.78", 0);
    assertEndpointIsParsedAsExpected("[::ffff:12.34.56.78]:777", "::ffff:12.34.56.78", 777);
}

TEST_F(SocketAddress, fromUserInput)
{
    ASSERT_EQ(socketAddressFromUserInput("").isNull(), true);
    ASSERT_EQ(socketAddressFromUserInput(":7001").isNull(), true);
    ASSERT_EQ(socketAddressFromUserInput("localhost").isNull(), false);
    ASSERT_EQ(socketAddressFromUserInput("localhost:7001").isNull(), false);
}

TEST_F(SocketAddress, compatible_with_nx_fusion)
{
    ASSERT_EQ(
        R"({"address":"localhost","port":12345})",
        QJson::serialized(nx::network::SocketAddress("localhost", 12345)));

    nx::network::SocketAddress value;
    ASSERT_TRUE(QJson::deserialize(nx::Buffer(R"({"address":"localhost","port":12345})"), &value));
    ASSERT_EQ(nx::network::SocketAddress("localhost", 12345), value);
}

//-------------------------------------------------------------------------------------------------

TEST(KeepAliveOptions, parse)
{
    using namespace std::chrono;

    ASSERT_EQ(
        KeepAliveOptions(seconds(11), seconds(131), 12),
        KeepAliveOptions::fromString("{11, 131, 12}"));

    ASSERT_EQ(
        KeepAliveOptions(milliseconds(11), milliseconds(131), 12),
        KeepAliveOptions::fromString("{11ms, 131ms, 12}"));
}

TEST(KeepAliveOptions, toString)
{
    using namespace std::chrono;

    ASSERT_EQ(
        "{11,131,12}",
        KeepAliveOptions(seconds(11), seconds(131), 12).toString());

    ASSERT_EQ(
        "{11ms,131ms,12}",
        KeepAliveOptions(milliseconds(11), milliseconds(131), 12).toString());
}

} // namespace nx::network::test
