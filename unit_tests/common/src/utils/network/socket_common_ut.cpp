#include <gtest/gtest.h>
#include <utils/network/socket_common.h>

#include <algorithm>

static const auto ipV6 = QByteArray::fromHex("00000000000000000000FFFF7F000001");

static void checkLocalhost(const HostAddress& host)
{
    EXPECT_EQ(0x0100007F,   host.inAddr().s_addr);
    EXPECT_EQ(0x7F000001,   host.ipv4());
    EXPECT_EQ(ipV6.toHex(), host.ipv6().toHex());
}

TEST(UtilsNetworkSocketCommon, HostAddressLocalhost)
{
    checkLocalhost(HostAddress("localhost"));
    checkLocalhost(HostAddress("127.0.0.1"));

    struct in_addr addr = { 0x0100007F };
    checkLocalhost(HostAddress(addr));

    checkLocalhost(HostAddress(0x7F000001));
    checkLocalhost(HostAddress(ipV6));
}

TEST(UtilsNetworkSocketCommon, SocketAddressLocalhost)
{
    EXPECT_EQ(SocketAddress("localhost", 1234).toString(), QString("localhost:1234"));
    EXPECT_EQ(SocketAddress("127.0.0.1", 4321).toString(), QString("127.0.0.1:4321"));
}
