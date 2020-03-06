#include "discovery_manager.h"

#include <QCryptographicHash>

#include <nx/utils/log/log.h>

#include "plugin.h"
#include "camera_manager.h"
#include "camera/camera.h"
#include "device/video/utils.h"
#include "device/audio/utils.h"

namespace nx::usb_cam {

namespace {

static constexpr const char kVendorName[] = "usb_cam";

std::string buildNxId(const device::DeviceData& device)
{
    std::string nxId = device.uniqueId;

    // If nxId is still empty, fall back to volatile device path.
    if (nxId.empty())
        nxId = device.path;

#ifdef Q_OS_WIN
    HW_PROFILE_INFO HwProfInfo;
    if (GetCurrentHwProfile(&HwProfInfo))
        nxId += QString::fromUtf16((const ushort*) HwProfInfo.szHwProfileGuid).toStdString();
#endif

    // Convert the id to one guaranteed to work with the media server - no special characters.
    nxId = QCryptographicHash::hash(
        nxId.c_str(),
        QCryptographicHash::Md5).toHex().constData();
    return nxId;
}

}

DiscoveryManager::DiscoveryManager(
    nxpt::CommonRefManager* const refManager,
    nxpl::TimeProvider* const timeProvider)
    :
    m_refManager(refManager),
    m_timeProvider(timeProvider)
{
    findCameras(nullptr, nullptr);
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

int DiscoveryManager::addRef() const
{
    return m_refManager.addRef();
}

int DiscoveryManager::releaseRef() const
{
    return m_refManager.releaseRef();
}

void DiscoveryManager::getVendorName(char* buf) const
{
    strncpy(buf, kVendorName, nxcip::MAX_TEXT_LEN - 1);
}

int DiscoveryManager::findCameras(nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr)
{
    NX_DEBUG(this, "Start camera search...");
    std::vector<device::DeviceData> devices = device::video::getDeviceList();
    device::audio::selectAudioDevices(devices);
    for (auto & device: devices)
    {
        device.uniqueId = buildNxId(device);
        addOrUpdateCamera(device);
    }

    int count = std::min((int)devices.size(), nxcip::CAMERA_INFO_ARRAY_SIZE);
    NX_DEBUG(this, "Camera search completed, found %1 cameras", count);
    if (!cameras)
        return count;

    for (int i = 0; i < count; ++i)
    {
        strncpy(cameras[i].modelName, devices[i].name.c_str(), sizeof(cameras[i].modelName) - 1);
        strncpy(cameras[i].url, localInterfaceIPAddr, sizeof(cameras[i].url) - 1);
        strncpy(cameras[i].uid, devices[i].uniqueId.c_str(), sizeof(cameras[i].uid) - 1);
        strncpy(cameras[i].auxiliaryData, devices[i].audioPath.c_str(), sizeof(cameras[i].auxiliaryData) - 1);
    }

    return count;
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
    CameraAndDeviceData* cameraData = getCameraAndDeviceData(info.uid);

    if (!cameraData)
        return nullptr;

    if (!cameraData->camera)
        cameraData->camera = std::make_shared<Camera>(cameraData->device.path, info, m_timeProvider);

    return new CameraManager(cameraData->camera);
}

int DiscoveryManager::getReservedModelList(char** /*modelList*/, int* count)
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}

void DiscoveryManager::addOrUpdateCamera(const device::DeviceData& device)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cameras.find(device.uniqueId);
    if (it == m_cameras.end())
    {
        NX_DEBUG(this, "Found new device: %1", device.toString());
        m_cameras.emplace(device.uniqueId, CameraAndDeviceData(device));
        return;
    }

    if (it->second.device == device)
    {
        NX_DEBUG(this, "Device already found: %1", device.toString());
        return;
    }

    it->second.device = device;
    if (it->second.camera)
    {
        NX_DEBUG(this, "Device with nxId already found but device changed: old: %1, new: %2",
            it->second.device.toString(), device.toString());
        it->second.camera->videoStream().updateUrl(it->second.device.path);
        it->second.camera->audioStream().updateUrl(it->second.device.audioPath);
    }
}

DiscoveryManager::CameraAndDeviceData* DiscoveryManager::getCameraAndDeviceData(
    const std::string& nxId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cameras.find(nxId);
    return it != m_cameras.end() ? &it->second : nullptr;
}

} // namespace nx::usb_cam
