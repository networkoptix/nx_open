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
    const QString id = "SomeUpnpId";
    const std::optional<int> versions[] = {std::nullopt, 1, 2};
    for (const auto& version: versions)
    {
        const QString urn = version
            ? toUpnpUrn(id, "xxx", *version)
            : toUpnpUrn(id, "xxx");
        const QString expected = NX_FMT("urn:schemas-upnp-org:xxx:SomeUpnpId:%1",
            version.value_or(1));
        EXPECT_EQ(urn, expected);
        EXPECT_EQ(id, fromUpnpUrn(urn, u"xxx"));
        EXPECT_EQ(id, fromUpnpUrn(urn, u"xxx", version.value_or(1)));
        EXPECT_NE(id, fromUpnpUrn(urn, u"xxx", version.value_or(1) + 1));
        EXPECT_NE(id, fromUpnpUrn(urn, u"yyy"));
    }
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
