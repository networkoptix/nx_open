#ifdef _WIN32
#include "dshow_utils.h"

#include <map>

#include "device/device_data.h"
#include "device/video/windows/dshow_utils.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace audio {
namespace detail {

void fillCameraAuxiliaryData(nxcip::CameraInfo* cameras, int cameraCount)
{
    std::vector<video::detail::AudioDeviceDescriptor> devices =
        video::detail::getAudioDeviceList();
    if (devices.empty())
        return;

    std::vector<video::detail::AudioDeviceDescriptor*> defaults;
    std::map<video::detail::AudioDeviceDescriptor*, bool> audioTaken;
    for (int i = 0; i < devices.size(); ++i)
    {
        if (devices[i].isDefault())
            defaults.push_back(&devices[i]);

        audioTaken.emplace(&devices[i], false);
    }

    std::vector<nxcip::CameraInfo*> muteCameras;
    for (auto camera = cameras; camera < cameras + cameraCount; ++camera)
    {
        bool mute = true;
        for (auto device = devices.data(); device < devices.data() + devices.size(); ++device)
        {
            if (audioTaken[device])
                continue;

            if (device->data.name.find(camera->modelName) != std::string::npos)
            {
                mute = false;
                audioTaken[device] = true;

                strncpy(
                    camera->auxiliaryData,
                    device->data.path.c_str(),
                    sizeof(camera->auxiliaryData) - 1);

                break;
            }
        }
        if (mute)
            muteCameras.push_back(camera);
    }

    if (!muteCameras.empty() && !defaults.empty())
    {
        for (const auto & muteCamera : muteCameras)
        {
            strncpy(
                muteCamera->auxiliaryData,
                defaults[0]->data.name.c_str(),
                sizeof(muteCamera->auxiliaryData) - 1);
        }
    }
}

bool pluggedIn(const std::string& devicePath)
{
    auto devices = video::detail::getAudioDeviceList();
    for (const auto & device : devices)
    {
        if (device.data.path == devicePath)
            return true;
    }
    return false;
}

} // namespace detail
} // namespace audio
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif //_WIN32
