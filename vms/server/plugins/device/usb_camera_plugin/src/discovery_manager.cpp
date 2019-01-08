#include "discovery_manager.h"

#include <QCryptographicHash>

#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>

#include "plugin.h"
#include "camera_manager.h"
#include "camera/camera.h"
#include "device/video/utils.h"
#include "device/audio/utils.h"

namespace nx {
namespace usb_cam {

namespace {

static constexpr const char kVendorName[] = "usb_cam";

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
    std::vector<DeviceDataWithNxId> devices = findCamerasInternal();

    int i;
    for (i = 0; i < devices.size() && i < nxcip::CAMERA_INFO_ARRAY_SIZE; ++i)
    {
        strncpy(
            cameras[i].modelName,
            devices[i].device.name.c_str(),
            sizeof(cameras[i].modelName) - 1);
        strncpy(cameras[i].url, localInterfaceIPAddr, sizeof(cameras[i].url) - 1);
        strncpy(cameras[i].uid, devices[i].nxId.c_str(), sizeof(cameras[i].uid) - 1);
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
    CameraAndDeviceDataWithNxId * cameraData = getCameraAndDeviceData(info.uid);

    if (!cameraData)
        return nullptr;

    if (!cameraData->camera)
        cameraData->camera = std::make_shared<Camera>(this, info, m_timeProvider);

    return new CameraManager(cameraData->camera);
}

int DiscoveryManager::getReservedModelList(char** /*modelList*/, int* count)
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}

void DiscoveryManager::addOrUpdateCamera(const DeviceDataWithNxId& device)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cameras.find(device.nxId);
    NX_DEBUG(this, "addOrUpdateCamera attempting to add device: %1", device.toString());
    if(it == m_cameras.end())
    {
        NX_DEBUG(this, "Found new device: %1", device.toString());
        m_cameras.emplace(device.nxId, CameraAndDeviceDataWithNxId(device));
    }
    else
    {
        if (it->second.deviceData != device)
        {
            NX_DEBUG(
                this,
                "Device with nxId already found but device changed: old: %1, new: %2",
                it->second.deviceData.toString(),
                device.toString());

            it->second.deviceData = device;
        }
        else
        {
            NX_DEBUG(
                this,
                "Device already found");
        }
    }
}

std::string DiscoveryManager::getFfmpegUrl(const std::string& nxId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cameras.find(nxId);
    return it != m_cameras.end() ? it->second.deviceData.device.path : std::string();
}

std::vector<DiscoveryManager::DeviceDataWithNxId> DiscoveryManager::findCamerasInternal()
{
    std::vector<device::DeviceData> devices = device::video::getDeviceList();
    std::vector<DeviceDataWithNxId> nxDevices;

    for (auto & device: devices)
    {
        std::string nxId = device.uniqueId;

        // If nxId is still empty, fall back to volatile device path.
        if (nxId.empty())
            nxId = device.path;

        // Convert the id to one guaranteed to work with the media server - no special characters.
        nxId = QCryptographicHash::hash(
            nxId.c_str(),
            QCryptographicHash::Md5).toHex().constData();

        DeviceDataWithNxId nxDevice(device, nxId);
        
        nxDevices.push_back(nxDevice);
        addOrUpdateCamera(nxDevice);
    }

    return nxDevices;
}

DiscoveryManager::CameraAndDeviceDataWithNxId* 
DiscoveryManager::getCameraAndDeviceData(const std::string& nxId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cameras.find(nxId);
    return it != m_cameras.end() ? &it->second : nullptr;
}

} // namespace nx 
} // namespace usb_cam 