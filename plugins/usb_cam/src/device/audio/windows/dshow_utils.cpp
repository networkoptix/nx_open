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

        audioTaken.insert(
            std::pair<video::detail::AudioDeviceDescriptor*, bool>(&devices[i], false));
    }

    std::vector<nxcip::CameraInfo*> muteCameras;
    for (auto camera = cameras; camera < cameras + cameraCount; ++camera)
    {
        bool mute = true;
        for (auto device = devices.data(); device < devices.data() + devices.size(); ++device)
        {
            if (audioTaken[device])
                continue;

            if(device->data.deviceName.find(camera->modelName) != std::string::npos)
            {
                mute = false;
                audioTaken[device] = true;
                
                 // Duplicate audio devices in windows are prepended with an index, making them
                 // unique. Therefore, it is safe to use the device name instead of its path. In
                 // fact, the path requires further parsing before use by ffmpeg, as ffmpeg
                 // replaces all ':' chars wtih '_' in audio alternative names. Oddly, it does not
                 // do this for video.
                strncpy(
                    camera->auxiliaryData,
                    device->data.deviceName.c_str(),
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
                defaults[0]->data.deviceName.c_str(),
                sizeof(muteCamera->auxiliaryData) - 1);
        }
    }
}

bool pluggedIn(const char * devicePath)
{
    auto devices = video::detail::getAudioDeviceList();
    for (const auto & device : devices)
    {
        if (strcmp(device.data.deviceName.c_str(), devicePath) == 0)
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