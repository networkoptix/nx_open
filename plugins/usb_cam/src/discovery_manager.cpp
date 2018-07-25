#include "discovery_manager.h"

#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#endif
#include <cstdio>

#include <nx/network/http/http_client.h>

#include "device/utils.h"
#include "camera_manager.h"
#include "plugin.h"

namespace nx {
namespace usb_cam {

DiscoveryManager::DiscoveryManager(nxpt::CommonRefManager* const refManager,
    nxpl::TimeProvider *const timeProvider)
    :
    m_refManager(refManager),
    m_timeProvider(timeProvider)
{
}

void* DiscoveryManager::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(&interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager)) == 0)
    {
        addRef();
        return this;
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int DiscoveryManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int DiscoveryManager::releaseRef()
{
    return m_refManager.releaseRef();
}

void DiscoveryManager::getVendorName(char* buf) const
{
    strcpy(buf, "usb_cam");
}

int DiscoveryManager::findCameras(nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr)
{
    std::vector<device::DeviceData> devices = device::getDeviceList();
    int deviceCount = devices.size();
    for (int i = 0; i < deviceCount && i < nxcip::CAMERA_INFO_ARRAY_SIZE; ++i)
    {
        strcpy(cameras[i].modelName, devices[i].deviceName.c_str());

        QByteArray url =
            QByteArray("webcam://").append(
            nx::utils::Url::toPercentEncoding(devices[i].devicePath.c_str()));
        strcpy(cameras[i].url, url.data());

        const QByteArray& uid = QCryptographicHash::hash(url, QCryptographicHash::Md5).toHex();
        strcpy(cameras[i].uid, uid.data());
    }
    return deviceCount;
}

int DiscoveryManager::checkHostAddress(nxcip::CameraInfo* /*cameras*/, const char* /*address*/, const char* /*login*/, const char* /*password*/)
{
    //host address doesn't mean anything for a local web cam
    return 0;
}

int DiscoveryManager::fromMDNSData(
    const char* /*discoveredAddress*/,
    const unsigned char* /*mdnsResponsePacket*/,
    int /*mdnsResponsePacketSize*/,
    nxcip::CameraInfo* /*cameraInfo*/)
{
    return nxcip::NX_NO_ERROR;
}

int DiscoveryManager::fromUpnpData(const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/)
{
    return nxcip::NX_NO_ERROR;
}

nxcip::BaseCameraManager* DiscoveryManager::createCameraManager(const nxcip::CameraInfo& info)
{
    return new CameraManager(info, m_timeProvider);
}

int DiscoveryManager::getReservedModelList(char** /*modelList*/, int* count)
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}

} // namespace nx 
} // namespace usb_cam 