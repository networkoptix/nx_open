#include "plugins/resource/upnp/upnp_device_searcher.h"
#include "plugins/resource/upnp/upnp_resource_searcher.h"
#include "plugins/resource/upnp/upnp_async_client.h"

#include <gtest.h>
#include <iostream>
#include <QFuture>

TEST(UPNP, Urn)
{
    const auto id = lit("SomeUpnpId");
    const auto urn = toUpnpUrn( id, lit("xxx") );
    EXPECT_EQ( urn, lit("urn:schemas-upnp-org:xxx:SomeUpnpId:1") );

    EXPECT_EQ( id, fromUpnpUrn( urn, lit("xxx") ) );
    EXPECT_NE( id, fromUpnpUrn( urn, lit("yyy") ) );
}

class RS : public UPNPSearchHandler
{
    virtual bool processPacket(
        const QHostAddress& localInterfaceAddress,
        const SocketAddress& discoveredDevAddress,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo ) override
    {
        std::cout << localInterfaceAddress.toString().toStdString()
                  << discoveredDevAddress.toString().toStdString()
                  << xmlDevInfo.data() << std::endl;

        return true;
    }
};

TEST(UPNP, DISABLED_DeviceSearcher)
{
    TimerManager timerManager;
    UPNPDeviceSearcher deviceSearcher(QLatin1String("InternetGatewayDevice"));

    RS rs;
    deviceSearcher.registerHandler(&rs);
    QThread::sleep(5);
}

TEST(UPNP, DISABLED_AsyncClient)
{
    auto client = std::make_shared<UpnpAsyncClient>();
    const QUrl url( lit("http://192.168.1.1:42692/ctl/IPConn") );

    UpnpAsyncClient::Message request;
    request.action = lit( "GetExternalIPAddress" );
    request.service = lit( "WANIpConnection" );

    QMutex mutex;
    mutex.lock();
    UpnpAsyncClient::Message response;
    client->doUpnp( url, request, [&](const UpnpAsyncClient::Message& message) {
        response = message;
        mutex.unlock();
    } );

    mutex.lock(); // TODO: replace with cond variable
    mutex.unlock();

    const QString EXTERNAL_IP = lit("NewExternalIPAddress");
    EXPECT_EQ( response.action, request.action + lit("Response") );
    EXPECT_EQ( response.params[EXTERNAL_IP], QString(lit("10..2.130")) );
}
