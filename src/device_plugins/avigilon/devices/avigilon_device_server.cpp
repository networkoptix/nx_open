#include "avigilon_device_server.h"
#include "avigilon_device.h"

AVigilonDeviceServer::AVigilonDeviceServer()
{

}

AVigilonDeviceServer::~AVigilonDeviceServer()
{

};

AVigilonDeviceServer& AVigilonDeviceServer::instance()
{
    static AVigilonDeviceServer inst;
    return inst;
}

bool AVigilonDeviceServer::isProxy() const
{
    return false;
}

QString AVigilonDeviceServer::name() const
{
    return "Avigilon";
}


CLDeviceList AVigilonDeviceServer::findDevices()
{
    CLDeviceList lst;


    CLAvigilonDevice* dev = new CLAvigilonDevice();
    dev->setIP(QHostAddress("192.168.1.99"), false);
    dev->setMAC("00-18-85-00-C8-72");
    dev->setUniqueId(dev->getMAC());
    dev->setName("clearpix");
    dev->setAuth("admin", "admin");

    lst[dev->getUniqueId()] = dev;

    return lst;
}
