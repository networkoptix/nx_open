#include <gtest/gtest.h>
#include <test_support/mediaserver_launcher.h>
#include <nx/network/nettools.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace mediaserver {
namespace test {
namespace network {

static QList<SocketAddress> allLocalAddresses(int port)
{
    AddressFilters addressMask =
        AddressFilter::ipV4
        | AddressFilter::ipV6
        | AddressFilter::noLocal
        | AddressFilter::noLoopback;

    QList<SocketAddress> serverAddresses;
    for (const auto& host: allLocalAddresses(addressMask))
        serverAddresses << SocketAddress(host, port);

    return serverAddresses;
}

TEST(ServerListensBothIpv6AndIpv4, main)
{
    HostAddress a("fe80::d13a:fe23:f0d7:65d4%0");
    if ((bool)a.ipV6().first)
        qWarning() << "Resolved!";
    else
        qWarning() << "NOT resolved";

    std::unique_ptr<MediaServerLauncher> mediaServerLauncher;
    mediaServerLauncher.reset(new MediaServerLauncher());
    ASSERT_TRUE(mediaServerLauncher->start());

    auto addressesToTest = allLocalAddresses(mediaServerLauncher->port());
    EXPECT_FALSE(addressesToTest.isEmpty());

    bool ipv4AddressPresent = false;
    bool ipv6AddressPresent = false;

    qWarning() << "port is" << mediaServerLauncher->port();
    // std::this_thread::sleep_for(std::chrono::minutes(5));

    nx_http::HttpClient httpClient;
    nx::utils::Url testUrl("http://host:111/static/index.html");
    testUrl.setPort(mediaServerLauncher->port());

    for (const auto& addr: addressesToTest)
    {
        qWarning() << "Testing addr" << addr.address.toString();
        if (addr.address.isPureIpV6())
        {
            ipv6AddressPresent = true;
            qWarning() << "IS IPV6";
        }
        else if ((bool) addr.address.ipV4())
        {
            ipv4AddressPresent = true;
            // testUrl.setHost(addr.address.toString());
            qWarning() << "IS IPV4";
        }
        else
        {
            ASSERT_TRUE(false);
        }

        qWarning() << "TEST ADDRESS" << addr.address.toString();
        testUrl.setHost(addr.address.toString());
        qWarning() << "TEST URL" << testUrl.toString();

        ASSERT_TRUE(httpClient.doGet(testUrl));
        qWarning() << "Success";
    }

    EXPECT_TRUE(ipv4AddressPresent);
    EXPECT_TRUE(ipv6AddressPresent);
}

} // namespace network
} // namespace test
} // namespace mediaserver
} // namespace nx
