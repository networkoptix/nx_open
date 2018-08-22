#ifdef __linux__
#pragma once

#include <string>
#include <vector>

#include <camera/camera_plugin.h>

#include "audio_discovery_manager.h"

namespace nx {
namespace device {

class AlsaAudioDiscoveryManager : public AudioDiscoveryManagerPrivate
{
private:
    enum IOType
    {
        kNone = 0,
        kInput = 1,
        kOutput = 2,
        kInputOutput = kInput | kOutput
    };

    struct DeviceDescriptor
    {
        std::string path;
        std::string name;
        int cardIndex;
        IOType ioType;
        bool isDefault;
        bool sysDefault;

        DeviceDescriptor():
            ioType(kNone),
            isDefault(false)
        {   
        }

       bool operator==(const DeviceDescriptor& rhs) const
        {
            return 
                path == rhs.path
                && name == rhs.name
                && cardIndex == rhs.cardIndex
                && ioType == rhs.ioType
                && isDefault == rhs.isDefault;
        } 

        bool isCameraAudioInput(const nxcip::CameraInfo& info) const
        {
            return name.find(info.modelName) != name.npos && isInput();
        }

        bool isInput() const
        {
            return ioType & kInput;
        }
    };

public:
    void fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const;

private:
    std::vector<DeviceDescriptor> getDevices() const;
};

} // namespace device
} // namespace nx

#endif