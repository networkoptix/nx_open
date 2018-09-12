#ifdef __linux__
#include "alsa_audio_discovery_manager.h"

#include <limits.h>
#include <algorithm>
#include <map>

#include <alsa/asoundlib.h>

namespace nx {
namespace device {

namespace {

    std::vector<std::string> kMotherBoardAudioList = {
        "HDA Intel PCH"
        // AMD? builtin audio card goes here
    };

} //namespace

void AlsaAudioDiscoveryManager::fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const
{
    std::vector<DeviceDescriptor> devices = getDevices();
    std::map<DeviceDescriptor*, bool> audioTaken;
    std::vector<DeviceDescriptor*> defaults;
    std::vector<nxcip::CameraInfo*> muteCameras;

    for (int i = 0; i < devices.size(); ++i)
    {
        audioTaken.insert(std::pair<DeviceDescriptor*, bool>(&devices[i], false));

        if ((devices[i].isDefault || devices[i].sysDefault) && devices[i].isInput())
            defaults.push_back(&devices[i]);
    }

    for (auto camera = cameras; camera < cameras + cameraCount; ++camera)
    {
        bool mute = true;
        for (int j = 0; j < devices.size(); ++j)
        {
            DeviceDescriptor * device = &devices[j];
            
            if (!audioTaken[device] && device->isCameraAudioInput(*camera) && device->sysDefault)
            {
                mute = false;
                audioTaken[device] = true;
                strncpy(
                    camera->auxiliaryData,
                    device->path.c_str(),
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
                defaults[0]->path.c_str(),
                sizeof(muteCamera->auxiliaryData) - 1);
        }
    }
}

std::vector<AlsaAudioDiscoveryManager::DeviceDescriptor> 
AlsaAudioDiscoveryManager::getDevices() const
{
    std::vector<DeviceDescriptor> motherBoardDevices;
    std::vector<DeviceDescriptor> devices;
    int card = -1;

    while(snd_card_next(&card) == 0 && card != -1)
    {
        char * cardName;
        snd_card_get_name(card, &cardName);
        char ** hints;
        if (snd_device_name_hint(card, "pcm", (void***)&hints) != 0)
            continue;

        char ** hint = hints;
        for (; *hint; ++hint)
        {
            DeviceDescriptor device;
            if (cardName)
                device.name = cardName;

            device.cardIndex = card;

            if (char * nameHint = snd_device_name_get_hint(*hint, "NAME"))
            {
                device.path = nameHint;
                device.sysDefault = strstr(nameHint, "sysdefault") != nullptr;
                device.isDefault = !device.sysDefault && strstr(nameHint, "default") != nullptr;
                free(nameHint);
            }

            if (char * ioHint = snd_device_name_get_hint(*hint, "IOID"))
            {
                device.ioType = strcmp(ioHint, "Input") == 0 ? iotInput : iotOutput;
                free(ioHint);
            }
            else
            {
                device.ioType = iotInputOutput;
            }

            if ((device.isDefault || device.sysDefault))
            {
                bool builtin = false;
                for(const auto & motherBoardAudio : kMotherBoardAudioList)
                    builtin = device.name.find(motherBoardAudio) != std::string::npos;

                auto it = std::find(motherBoardDevices.begin(), motherBoardDevices.end(), device);
                if(builtin && it == motherBoardDevices.end())
                {
                    motherBoardDevices.push_back(device);
                }
                else if(std::find(devices.begin(), devices.end(), device) == devices.end())
                {
                    devices.push_back(device);
                }
            }
        }
        snd_device_name_free_hint((void**)hints);

        if (cardName)
            free(cardName);
    }

    /**
     * We want builtin mother board audio devices at the back of the list because they are
     * registered even with nothing plugged into them. Audio capture devices that are actually
     * present should get priority.
     */
    for(const auto motherBoardDevice : motherBoardDevices)
        devices.push_back(motherBoardDevice);

    return devices;
}

} // namespace device
} // namespace nx

#endif