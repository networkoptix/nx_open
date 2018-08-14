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

void listDevice(snd_ctl_t * audioHandle, int devNum);
void listCardInfo(snd_ctl_t * audioHandle);

void listAvailable(int card)
{
    std::string hwName = std::string("hw:") + std::to_string(card);
    
    snd_ctl_t * audioHandle;
    if(snd_ctl_open(&audioHandle, hwName.c_str(), 0) < 0)
        return;

    int devNum = -1;
    while(snd_ctl_pcm_next_device(audioHandle, &devNum) == 0 && devNum != -1)
    {
        listCardInfo(audioHandle);
        listDevice(audioHandle, devNum);
        std::cout << std::endl;
    }
        
    snd_ctl_close(audioHandle);
    snd_config_update_free_global();
}

void listCardInfo(snd_ctl_t * audioHandle)
{
    snd_ctl_card_info_t * cardInfo;
    snd_ctl_card_info_alloca(&cardInfo);
    snd_ctl_card_info(audioHandle, cardInfo);
    
    int card = snd_ctl_card_info_get_card(cardInfo);
    const char * id = snd_ctl_card_info_get_id(cardInfo);
    const char * name = snd_ctl_card_info_get_name(cardInfo);
    const char * longName = snd_ctl_card_info_get_longname(cardInfo);
    const char * comps = snd_ctl_card_info_get_components(cardInfo);
    const char * driver = snd_ctl_card_info_get_driver(cardInfo);

    std::cout << 
        "card: " << card <<
        ", id: " << id <<
        ", name: " << name <<
        ", longName: " << longName << 
        ", comps: " << comps <<
        ", driver: " << driver << std::endl;
}

void listDevice(snd_ctl_t * audioHandle, int devNum)
{
    snd_pcm_info_t * pcmInfo;
    snd_pcm_info_alloca(&pcmInfo);
    memset(pcmInfo, 0, snd_pcm_info_sizeof());

    snd_pcm_info_set_device(pcmInfo, devNum);
    snd_pcm_info_set_stream(pcmInfo, SND_PCM_STREAM_CAPTURE);
    snd_ctl_pcm_info(audioHandle, pcmInfo);

    int subDevCount = snd_pcm_info_get_subdevices_count(pcmInfo);
    int available = snd_pcm_info_get_subdevices_avail(pcmInfo);
    std::cout << "\tdev " << devNum << ": " << snd_pcm_info_get_id(pcmInfo);
    if(snd_pcm_info_get_name(pcmInfo))
        std::cout <<  ", " << snd_pcm_info_get_name(pcmInfo);
    std::cout <<std::endl;
    std::cout << "\tsubDevs total: " << subDevCount << ", available: " << available << std::endl;
    
    for(int i = 0; i < subDevCount; ++i)
    {
        snd_pcm_info_set_subdevice(pcmInfo, i);
        snd_ctl_pcm_info(audioHandle, pcmInfo);

        std::cout << "\t\tsubDev " << i << ": " << snd_pcm_info_get_subdevice_name(pcmInfo) << std::endl;
    }
}



} // namespace

///////////////////////////////////////////// Discovery ////////////////////////////////////////////

AlsaAudioDiscoveryManager::AlsaAudioDiscoveryManager()
{
}


AlsaAudioDiscoveryManager::~AlsaAudioDiscoveryManager()
{
    //snd_config_update_free_global();
}

void AlsaAudioDiscoveryManager::fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const
{
    std::vector<nxcip::CameraInfo*> muteCameras;
    std::vector<DeviceDescriptor> devices = getDevices();
    for (int i = 0; i < cameraCount; ++i)
    {
        bool mute = true;
        for (int j = 0; j < devices.size(); ++j)
        {
            if (devices[j].isCameraAudioInput(cameras[i]))
            {
                mute = false;
                strncpy(
                    cameras[i].auxiliaryData,
                    //devices[j].path.c_str(),
                    (std::string("hw:") + std::to_string(devices[j].cardIndex)).c_str(),
                    sizeof(cameras[i].auxiliaryData) - 1);
                break;
            }
        }
        if (mute)
            muteCameras.push_back(&cameras[i]);
    }

    for (const auto & muteCamera : muteCameras)
        strncpy(muteCamera->auxiliaryData, "default", sizeof(muteCamera->auxiliaryData) - 1);
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
                    device.isDefault = strstr(nameHint, "sysdefault") != nullptr;
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

                if (device.isDefault && std::find(devices.begin(), devices.end(), device) == devices.end())
                    devices.push_back(device);
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