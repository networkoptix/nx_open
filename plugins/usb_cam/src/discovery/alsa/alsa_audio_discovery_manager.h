#ifdef __linux__
#pragma once

#include <string>
#include <vector>

#include <camera/camera_plugin.h>

#include "../audio_discovery_manager.h"

namespace nx {
namespace device {

#include <string>
#include <vector>

class AlsaAudioDiscoveryManager : public AudioDiscoveryManagerPrivate
{
private:
    enum IOType
    {
        iotNone = 0,
        iotInput = 1,
        iotOutput = 2,
        iotInputOutput = iotInput | iotOutput
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
            ioType(iotNone),
            isDefault(false),
            sysDefault(false)
        {   
        }

       bool operator==(const DeviceDescriptor& rhs) const
        {
            return 
                path == rhs.path
                && name == rhs.name
                && cardIndex == rhs.cardIndex
                && ioType == rhs.ioType;
        } 

        bool isCameraAudioInput(const nxcip::CameraInfo& info) const
        {
            return name.find(info.modelName) != name.npos && isInput();
        }

        bool isInput() const
        {
            return ioType & iotInput;
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