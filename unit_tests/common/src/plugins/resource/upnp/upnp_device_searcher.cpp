#include "plugins/resource/upnp/upnp_device_searcher.h"
#include "plugins/resource/upnp/upnp_resource_searcher.h"

#include <gtest.h>
#include <iostream>

class RS : public UPNPSearchHandler
{
    virtual bool processPacket(
        const QHostAddress& localInterfaceAddress,
        const HostAddress& discoveredDevAddress,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo ) override
    {
        std::cout << localInterfaceAddress.toString().toStdString()
                  << discoveredDevAddress.toString().toStdString()
                  << xmlDevInfo.data() << std::endl;

        return true;
    }
};

TEST(UPNPDeviceSearcher, DISABLED_JustTest)
{
    TimerManager timerManager;
    UPNPDeviceSearcher deviceSearcher(std::list<QString>(1, QLatin1String("InternetGatewayDevice")));

    RS rs;
    deviceSearcher.registerHandler(&rs);
    QThread::sleep(5);
}
