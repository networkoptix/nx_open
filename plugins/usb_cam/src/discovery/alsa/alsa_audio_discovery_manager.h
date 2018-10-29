#ifdef __linux__
#pragma once

#include <string>
#include <vector>

#include <camera/camera_plugin.h>

#include "../audio_discovery_manager.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace detail {

class AlsaAudioDiscoveryManager: public AudioDiscoveryManagerPrivate
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
        // The unique name of the interface used to open the device for capture.
        std::string path;

        // The name of the sound card, used to determine if it should be ignored or deprioritized.
        std::string name;

        // The index of the sound card in the system as reported by ALSA.
        int cardIndex;

        // Can be Input, Output or both. Used to find input only devices.
        IOType ioType;
        
        // Set to true if path contains the word "default", but not "sysdefault". 
        // Presumeably this is the systems default audio device, though ALSA has no explicit 
        // interface to get it.
        bool isDefault;

        // Set to true if the path contains "sysdefault", marking this device descriptor as the 
        // default audio interface for this particular device.
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
    virtual void fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const override;
    virtual bool pluggedIn(const std::string & devicePath) const override;

private:
    std::vector<DeviceDescriptor> getDevices() const;
};

} // namespace detail
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif