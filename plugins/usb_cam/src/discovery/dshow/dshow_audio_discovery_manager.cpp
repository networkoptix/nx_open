#ifdef _WIN32

#include "dshow_audio_discovery_manager.h"

#include <map>

#include "device/dshow/dshow_utils.h"

namespace nx{
namespace device {

void DShowAudioDiscoveryManager::fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const
{
    std::vector<impl::AudioDeviceDescriptor> devices = impl::getAudioDeviceList();
    if (devices.empty())
        return;

    std::vector<impl::AudioDeviceDescriptor*> defaults;
    std::map<impl::AudioDeviceDescriptor*, bool> audioTaken;
    for (int i = 0; i < devices.size(); ++i)
    {
        if (devices[i].isDefault())
            defaults.push_back(&devices[i]);

        audioTaken.insert(std::pair<impl::AudioDeviceDescriptor*, bool>(&devices[i], false));
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
                /**
                 * Duplicate audio devices in windows are prepended with an index, making them
                 * unique. Therefore, it is safe to use the device name instead of its path. In
                 * fact, the path requires further parsing before use by ffmpeg, as ffmpeg replaces
                 * all ':' chars wtih '_' in audio alternative names. Oddly enough, it does not do
                 * this for video.
                 */
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

} // namespace device
} // namespace nx

#endif //_WIN32