#pragma once

#include <vector>
#include <mutex>
#include <map>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>

#include "device/device_data.h"

namespace nx {
namespace usb_cam {

class DiscoveryManager: public nxcip::CameraDiscoveryManager
{
private:
    struct DeviceDataWithNxId
    {
        device::DeviceData device;
        std::string nxId;

        DeviceDataWithNxId(const device::DeviceData& device, const std::string& nxId):
        device(device),
        nxId(nxId)
        {
        }

        bool operator==(const DeviceDataWithNxId& rhs) const
        {
            return 
                nxId == rhs.nxId 
                && device.name == rhs.device.name 
                && device.path == rhs.device.path 
                && device.uniqueId == rhs.device.uniqueId;
        }

        bool operator!=(const DeviceDataWithNxId& rhs) const
        {
            return !operator==(rhs);
        }

        std::string toString() const
        {
            return
                std::string("nxId: ") + nxId
                + ", device: { name: " + device.name
                + ", path: " + device.path
                + ", uid: " + device.uniqueId + " }";
        }
    };

public:
    DiscoveryManager(nxpt::CommonRefManager* const refManager,
                     nxpl::TimeProvider *const timeProvider);

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    virtual void getVendorName( char* buf ) const override;
    virtual int findCameras(
        nxcip::CameraInfo* cameras,
        const char* localInterfaceIPAddr) override;
    virtual int checkHostAddress(
        nxcip::CameraInfo* cameras,
        const char* address,
        const char* login,
        const char* password) override;
    virtual int fromMDNSData(
        const char* discoveredAddress,
        const unsigned char* mdnsResponsePacket,
        int mdnsResponsePacketSize,
        nxcip::CameraInfo* cameraInfo ) override;
    virtual int fromUpnpData(
        const char* upnpXMLData,
        int upnpXMLDataSize,
        nxcip::CameraInfo* cameraInfo) override;
    virtual nxcip::BaseCameraManager* createCameraManager( const nxcip::CameraInfo& info ) override;
    virtual int getReservedModelList( char** modelList, int* count ) override;
    void addOrUpdateCamera(const DeviceDataWithNxId& nxDevice);
    std::string getFfmpegUrl(const std::string& nxId) const;

private:
    nxpt::CommonRefManager m_refManager;
    nxpl::TimeProvider *const m_timeProvider;
    mutable std::mutex m_mutex;
    std::map<std::string/*nxId*/, DeviceDataWithNxId> m_cameras;

private:
    std::vector<DeviceDataWithNxId> findCamerasInternal();
};

} // namespace nx 
} // namespace usb_cam 
