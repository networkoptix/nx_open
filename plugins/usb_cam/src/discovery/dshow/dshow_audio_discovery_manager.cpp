#ifdef _WIN32

#include "dshow_audio_discovery_manager.h"
#include "device/dshow/dshow_utils.h"

namespace nx{
namespace device {

DShowAudioDiscoveryManager::DShowAudioDiscoveryManager()
{
}


DShowAudioDiscoveryManager::~DShowAudioDiscoveryManager()
{
}

void DShowAudioDiscoveryManager::fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const
{
    std::vector<DShowAudioDiscoveryManager::DeviceDescriptor> devices = getDevices();
    std::vector<DShowAudioDiscoveryManager::DeviceDescriptor*> defaults;
    std::vector<nxcip::CameraInfo*> muteCameras;

    for (int i = 0; i < cameraCount; ++i)
    {
        bool mute = true;
        for (int j = 0; j < devices.size(); ++j)
        {
            if (devices[j].isDefault())
            {
                if (std::find(defaults.begin(), defaults.end(), &devices[j]) == defaults.end())
                    defaults.push_back(&devices[j]);
            }

            if (devices[j].data.deviceName.find(cameras[i].modelName))
            {
                mute = false;
                strncpy(
                    cameras[i].auxiliaryData,
                    devices[j].data.deviceName.c_str(),
                    sizeof(cameras[i].auxiliaryData) - 1);
                break;
            }
        }
        if (mute)
            muteCameras.push_back(&cameras[i]);
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

std::vector<DShowAudioDiscoveryManager::DeviceDescriptor>
DShowAudioDiscoveryManager::getDevices() const
{
    std::vector<DeviceData> devices = impl::getDeviceList(CLSID_AudioInputDeviceCategory);
    std::vector<DShowAudioDiscoveryManager::DeviceDescriptor> descriptors;
    for (const auto& device : devices)
        descriptors.push_back(device);
    return descriptors;
}

} // namespace device
} // namespace nx

#endif //_WIN32