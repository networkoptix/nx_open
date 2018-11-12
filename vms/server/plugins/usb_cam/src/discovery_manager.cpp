#include "discovery_manager.h"

#include <QCryptographicHash>
#include <QNetworkInterface>

#include <nx/utils/app_info.h>

#include "plugin.h"
#include "camera_manager.h"
#include "device/video/utils.h"
#include "device/video/rpi/rpi_utils.h"
#include "device/audio/utils.h"

namespace nx {
namespace usb_cam {

namespace {

static constexpr const char * const kVendorName = "usb_cam";
static constexpr const char * const kQtMacAddressDelimiter = ":";
static constexpr const char * const kNxMacAddressDelimiter = "-";

static std::string getEthernetMacAddress()
{
    for (const auto & iFace : QNetworkInterface::allInterfaces())
    {
        if(iFace.type() == QNetworkInterface::Ethernet)
        {
            // The media server modifies the mac address delimiter from ":" to "-",
            // so do it preemptively to avoid adding the unique id with the wrong delimiter.
            return iFace.hardwareAddress().replace(
                kQtMacAddressDelimiter, 
                kNxMacAddressDelimiter).toStdString();
        }
    }
    return {};
}

bool isRpiMmal(const std::string deviceName)
{
   return device::video::rpi::isRpi() && device::video::rpi::isMmalCamera(deviceName);
}

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
        // Convert camera uniqueId to one guaranteed to work with the media server.
        // On Raspberry Pi for the integrated camera, use the ethernet mac address per VMS-12076.
        if (isRpiMmal(devices[i].deviceName))
        {
            devices[i].uniqueId = getEthernetMacAddress();
        }
        else
        {
            const QByteArray& uidHash = QCryptographicHash::hash(
                devices[i].uniqueId.c_str(),
                QCryptographicHash::Md5).toHex();
            devices[i].uniqueId = uidHash.constData();
        }

        addFfmpegUrl(devices[i].uniqueId, devices[i].devicePath);
    }
   
    return devices;
}

} // namespace nx 
} // namespace usb_cam 