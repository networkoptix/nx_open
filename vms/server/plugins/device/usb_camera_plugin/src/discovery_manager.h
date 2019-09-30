#pragma once

#include <map>
#include <mutex>
#include <vector>
#include <memory>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>

#include "device/device_data.h"

namespace nx {
namespace usb_cam {

class Camera;

class DiscoveryManager: public nxcip::CameraDiscoveryManager
{
private:
    struct CameraAndDeviceData
    {
        device::DeviceData device;
        std::shared_ptr<Camera> camera;

        CameraAndDeviceData(const device::DeviceData& deviceData):
            device(deviceData)
        {
        }
    };

public:
    DiscoveryManager(nxpt::CommonRefManager* const refManager,
                     nxpl::TimeProvider *const timeProvider);

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual int addRef() const override;
    virtual int releaseRef() const override;

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
    void addOrUpdateCamera(const device::DeviceData& device);

private:
    nxpt::CommonRefManager m_refManager;
    nxpl::TimeProvider *const m_timeProvider;
    mutable std::mutex m_mutex;
    std::map<std::string/*nxId*/, CameraAndDeviceData> m_cameras;

private:
    //std::vector<DeviceDataWithNxId> findCamerasInternal();
    CameraAndDeviceData* getCameraAndDeviceData(const std::string& nxId);
};

} // namespace nx
} // namespace usb_cam
