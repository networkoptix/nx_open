// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <iostream>

#include <gtest/gtest.h>

#include <nx/network/upnp/upnp_device_searcher.h>

namespace nx {
namespace network {
namespace upnp {
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
    TestSearchHandler(DeviceSearcher* deviceSearcher):
        SearchAutoHandler(deviceSearcher, QLatin1String("InternetGatewayDevice"))
    {
    }

    virtual bool processPacket(
        const QHostAddress& localInterfaceAddress,
        const SocketAddress& discoveredDevAddress,
        const DeviceInfo& /*devInfo*/,
        const QByteArray& xmlDevInfo) override
    {
        std::cout << localInterfaceAddress.toString().toStdString()
            << discoveredDevAddress.toString()
            << xmlDevInfo.data() << std::endl;

        return true;
    }

    virtual bool isEnabled() const override
    {
        return true;
    }

};

// TODO: implement over mocked sockets
TEST(UpnpDeviceSearcher, DISABLED_General)
{
    nx::utils::TimerManager timerManager;
    DeviceSearcher deviceSearcher(
        &timerManager, std::make_unique<DeviceSearcherDefaultSettings>());
    TestSearchHandler testSearcher(&deviceSearcher);
    QThread::sleep(5);
}

} // namespace test
} // namespace upnp
} // namespace network
} // namespace nx
