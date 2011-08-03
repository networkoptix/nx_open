#include "iqeye_device_server.h"
#include "iqeye_device.h"

IQEyeDeviceServer::IQEyeDeviceServer()
{

}

IQEyeDeviceServer::~IQEyeDeviceServer()
{

};

IQEyeDeviceServer& IQEyeDeviceServer::instance()
{
    static IQEyeDeviceServer inst;
    return inst;
}

bool IQEyeDeviceServer::isProxy() const
{
    return false;
}

QString IQEyeDeviceServer::name() const
{
    return QLatin1String("IQEye");
}


CLDeviceList IQEyeDeviceServer::findDevices()
{
    CLDeviceList lst;

    CLIQEyeDevice::findDevices(lst);
    return lst;
}
