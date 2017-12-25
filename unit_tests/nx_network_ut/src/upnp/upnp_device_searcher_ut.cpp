#include <iostream>

#include <gtest/gtest.h>

#include <nx/network/upnp/upnp_device_searcher.h>

namespace nx_upnp {
namespace test {

TEST(Upnp, Urn)
{
    const auto id = lit("SomeUpnpId");
    const auto urn = toUpnpUrn(id, lit("xxx"));
    EXPECT_EQ(urn, lit("urn:schemas-upnp-org:xxx:SomeUpnpId:1"));

    EXPECT_EQ(id, fromUpnpUrn(urn, lit("xxx")));
    EXPECT_NE(id, fromUpnpUrn(urn, lit("yyy")));
}

class TestSearchHandler:
    public SearchAutoHandler
{
public:
    TestSearchHandler():
        SearchAutoHandler(QLatin1String("InternetGatewayDevice"))
    {
    }

    virtual bool processPacket(
        const QHostAddress& localInterfaceAddress,
        const SocketAddress& discoveredDevAddress,
        const DeviceInfo& /*devInfo*/,
        const QByteArray& xmlDevInfo) override
    {
        std::cout << localInterfaceAddress.toString().toStdString()
            << discoveredDevAddress.toString().toStdString()
            << xmlDevInfo.data() << std::endl;

        return true;
    }
};

// TODO: implement over mocked sockets
TEST(UpnpDeviceSearcher, DISABLED_General)
{
    nx::utils::TimerManager timerManager;
    DeviceSearcherDefaultSettings deviceSearcherSettings;
    DeviceSearcher deviceSearcher(deviceSearcherSettings);
    TestSearchHandler testSearcher;
    QThread::sleep(5);
}

} // namespace test
} // namespace nx_upnp
