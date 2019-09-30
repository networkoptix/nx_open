#ifdef _WIN32
#include "dshow_utils.h"

#include <map>

#include <nx/utils/log/log.h>

#include "device/device_data.h"
#include "device/video/windows/dshow_utils.h"

namespace nx::usb_cam::device::audio::detail {

void selectAudioDevices(std::vector<DeviceData>& devices)
{
    std::vector<video::detail::AudioDeviceDescriptor> audioDevices =
        video::detail::getAudioDeviceList();
    if (audioDevices.empty())
        return;

    std::vector<video::detail::AudioDeviceDescriptor*> defaults;
    std::map<video::detail::AudioDeviceDescriptor*, bool> audioTaken;
    for (int i = 0; i < audioDevices.size(); ++i)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Found audio device, name [%1], path: %2",
            audioDevices[i].data.name, audioDevices[i].data.path);
        if (audioDevices[i].isDefault())
            defaults.push_back(&audioDevices[i]);

        audioTaken.emplace(&audioDevices[i], false);
    }

    for (auto& device: devices)
    {
        bool mute = true;
        for (auto audioDevice = audioDevices.data(); audioDevice < audioDevices.data() + audioDevices.size(); ++audioDevice)
        {
            if (audioTaken[audioDevice])
                continue;

            if (audioDevice->data.name.find(device.name) != std::string::npos)
            {
                mute = false;
                audioTaken[audioDevice] = true;
                device.audioPath = audioDevice->data.path;
                break;
            }
        }
        if (mute && !defaults.empty())
            device.audioPath = defaults[0]->data.path;

        NX_DEBUG(NX_SCOPE_TAG, "Selected audio device: [%1]", device.toString());
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

void uninitialize()
{
}

} // namespace nx::usb_cam::device::audio::detail

#endif //_WIN32
