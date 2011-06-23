#include "avi_bluray_device.h"
#include "avi_strem_reader.h"

CLAviBluRayDevice::CLAviBluRayDevice(const QString& file):
    CLAviDevice(file)
{
}

CLAviBluRayDevice::~CLAviBluRayDevice()
{
}

CLStreamreader* CLAviBluRayDevice::getDeviceStreamConnection()
{
    return 0; 
}

bool CLAviBluRayDevice::isAcceptedUrl(const QString& url)
{
    return false; // not implemented bluray behavior right now
}
