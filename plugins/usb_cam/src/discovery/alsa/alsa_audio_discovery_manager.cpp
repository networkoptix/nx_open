#ifdef __linux__
#include "alsa_audio_discovery_manager.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <algorithm>

#include <iostream>

#include <alsa/asoundlib.h>

namespace nx {
namespace device {

namespace {

const std::vector<const char *> DEVICE_TYPES = 
{
    //"card",
    "pcm"
    //,"seq" , "hwdep", "mixer", "rawmidi", "timer"
};

} // namespace

///////////////////////////////////////////// Discovery ////////////////////////////////////////////

void AlsaAudioDiscoveryManager::fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const
{
    std::vector<nxcip::CameraInfo*> muteCameras;
    std::vector<DeviceDescriptor> devices = getDevices();
    std::vector<DeviceDescriptor*> defaults;

   for (int i = 0; i < cameraCount; ++i)
   {
        bool mute = true;
        for (int j = 0; j < devices.size(); ++j)
        {
            if(devices[j].isDefault && devices[j].isInput())
            {
                if(std::find(defaults.begin(), defaults.end(), &devices[j]) == defaults.end())
                    defaults.push_back(&devices[j]);
            }

            if (devices[j].isCameraAudioInput(cameras[i]) && devices[j].sysDefault)
            {
                mute = false;
                strncpy(
                    cameras[i].auxiliaryData,
                    devices[j].path.c_str(),
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
            strncpy(muteCamera->auxiliaryData, defaults[0]->path.c_str(), sizeof(muteCamera->auxiliaryData) - 1);
        }
    }
}

std::vector<AlsaAudioDiscoveryManager::DeviceDescriptor> 
AlsaAudioDiscoveryManager::getDevices() const
{
    std::vector<DeviceDescriptor> devices;
    int card = -1;
    while(snd_card_next(&card) == 0 && card != -1)
    {
        char * cardName;
        snd_card_get_name(card, &cardName);
        for (const auto & deviceType : DEVICE_TYPES)
        {
            char ** hints;
            if (snd_device_name_hint(card, deviceType, (void***)&hints) != 0)
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
                    device.ioType = strcmp(ioHint, "Input") == 0 ? kInput : kOutput;
                    free(ioHint);
                }
                else
                {
                    device.ioType = kInputOutput;
                }

                if ((device.isDefault || device.sysDefault) 
                    && std::find(devices.begin(), devices.end(), device) == devices.end())
                {
                    devices.push_back(device);   
                }
            }
            snd_device_name_free_hint((void**)hints);
        }
        if (cardName)
            free(cardName);
    }
    return devices;
}

} // namespace device
} // namespace nx

#endif