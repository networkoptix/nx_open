#ifdef _WIN32

#pragma once

#include "discovery/audio_discovery_manager.h"
#include "device/device_data.h"

namespace nx {
namespace device {
 
class DShowAudioDiscoveryManager : public AudioDiscoveryManagerPrivate{
public:
    DShowAudioDiscoveryManager();
    ~DShowAudioDiscoveryManager();
    virtual void fillCameraAuxData(nxcip::CameraInfo * cameras, int cameraCount) const override;


private:
    struct DeviceDescriptor
    {
        DeviceData data;

        DeviceDescriptor(const DeviceData & data):
            data(data)
        {
        }

        bool isDefault()
        {
            return data.devicePath == "0";
        }
    };

    std::vector<DeviceDescriptor> getDevices() const;
};

} // namespace device
} // namespace nx

#endif

