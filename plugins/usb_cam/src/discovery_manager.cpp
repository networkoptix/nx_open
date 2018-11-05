#include "discovery_manager.h"

#include <QCryptographicHash>

#include "plugin.h"
#include "camera_manager.h"
#include "device/video/utils.h"
#include "device/audio/utils.h"

namespace nx {
namespace usb_cam {

namespace {

constexpr char * const kVendorName = "usb_cam";

}

DiscoveryManager::DiscoveryManager(
    nxpt::CommonRefManager* const refManager,
    nxpl::TimeProvider *const timeProvider)
    :
    m_refManager(refManager),
    m_timeProvider(timeProvider)
{
    findCamerasInternal();
}

void* DiscoveryManager::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(
        &interfaceID,
        &nxcip::IID_CameraDiscoveryManager,
        sizeof(nxcip::IID_CameraDiscoveryManager)) == 0)
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
    strncpy(buf, kVendorName, nxcip::MAX_TEXT_LEN - 1);
}

int DiscoveryManager::findCameras(nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr)
{
    std::vector<device::DeviceData> devices = findCamerasInternal();
    
    int i;
    for (i = 0; i < devices.size() && i < nxcip::CAMERA_INFO_ARRAY_SIZE; ++i)
    {
        strncpy(
            cameras[i].modelName,
            devices[i].deviceName.c_str(),
            sizeof(cameras[i].modelName) - 1);
        strncpy(cameras[i].url, localInterfaceIPAddr, sizeof(cameras[i].url) - 1);
        strncpy(cameras[i].uid, devices[i].uniqueId.c_str(), sizeof(cameras[i].uid) - 1);
    }

    device::audio::fillCameraAuxiliaryData(cameras, i);

    return i;
}

int DiscoveryManager::checkHostAddress(
    nxcip::CameraInfo* /*cameras*/,
    const char* /*address*/,
    const char* /*login*/, 
    const char* /*password*/)
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

int DiscoveryManager::fromUpnpData(
    const char* /*upnpXMLData*/,
    int /*upnpXMLDataSize*/,
    nxcip::CameraInfo* /*cameraInfo*/)
{
    return nxcip::NX_NO_ERROR;
}

nxcip::BaseCameraManager* DiscoveryManager::createCameraManager(const nxcip::CameraInfo& info)
{
    return new CameraManager(this, m_timeProvider, info);
}

int DiscoveryManager::getReservedModelList(char** /*modelList*/, int* count)
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}

void DiscoveryManager::addFfmpegUrl(const std::string& uniqueId, const std::string& ffmpegUrl)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_ffmpegUrlMap.find(uniqueId);
    if(it == m_ffmpegUrlMap.end())
        m_ffmpegUrlMap.emplace(uniqueId, ffmpegUrl);
    else
        it->second = ffmpegUrl;
}

std::string DiscoveryManager::getFfmpegUrl(const std::string& uniqueId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_ffmpegUrlMap.find(uniqueId);
    return it != m_ffmpegUrlMap.end() ? it->second : std::string();
}

std::vector<device::DeviceData> DiscoveryManager::findCamerasInternal()
{
    std::vector<device::DeviceData> devices = device::video::getDeviceList();

    for (int i = 0; i < devices.size(); ++i)
    {
        const QByteArray& uidHash = QCryptographicHash::hash(
            devices[i].uniqueId.c_str(),
            QCryptographicHash::Md5).toHex();

        // Convert camera uniqueId to one guaranteed to work with the media server.
        devices[i].uniqueId = uidHash.constData();

        addFfmpegUrl(uidHash.constData(), devices[i].devicePath);
    }
   
    return devices;
}

} // namespace nx 
} // namespace usb_cam 