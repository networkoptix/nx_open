#ifdef __linux__
#include "alsa_utils.h"

#include <limits.h>
#include <algorithm>
#include <map>

#include <alsa/asoundlib.h>

namespace nx {
namespace usb_cam {
namespace device {
namespace audio {
namespace detail {

namespace {

static const std::vector<std::string> kMotherBoardAudioList = 
{
    "HDA Intel PCH"
    // AMD? built in audio card goes here
};

struct DeviceDescriptor
{
    enum IOType
    {
        iotNone = 0,
        iotInput = 1,
        iotOutput = 2,
        iotInputOutput = iotInput | iotOutput
    };

    // The unique name of the interface used to open the device for capture.
    std::string path;

    // The name of the sound card, used to match a camera to its microphone and to 
    //    determine if it should be ignored or deprioritized.
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
        std::string modelName(info.modelName);
        return 
            (modelName.find(name) != std::string::npos
            || name.find(modelName) != std::string::npos) 
            && isInput();
    }

    bool isInput() const
    {
        return ioType & iotInput;
    }
};

static std::vector<DeviceDescriptor> getDevices()
{
    std::vector<DeviceDescriptor> motherBoardDevices;
    std::vector<DeviceDescriptor> devices;
    int card = -1;

    while(snd_card_next(&card) == 0 && card != -1)
    {
        char * cardName = nullptr;
        snd_card_get_name(card, &cardName);
        char ** hints = nullptr;
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
                device.ioType = strcmp(ioHint, "Input") == 0 
                    ? DeviceDescriptor::iotInput 
                    : DeviceDescriptor::iotOutput;
                free(ioHint);
            }
            else
            {
                device.ioType = DeviceDescriptor::iotInputOutput;
            }

            if ((device.isDefault || device.sysDefault))
            {
                bool isMotherBoardAudio = false;
                for(const auto & motherBoardAudio : kMotherBoardAudioList)
                    isMotherBoardAudio = device.name.find(motherBoardAudio) != std::string::npos;

                auto it = std::find(motherBoardDevices.begin(), motherBoardDevices.end(), device);
                if(isMotherBoardAudio && it == motherBoardDevices.end())
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

    // We want built in mother board audio devices at the back of the list because they are
    // registered even with nothing plugged into them. Audio capture devices that are actually
    // present should get priority.
    for(const auto motherBoardDevice : motherBoardDevices)
        devices.push_back(motherBoardDevice);

    return devices;
}

} // namespace

void fillCameraAuxiliaryData(nxcip::CameraInfo* cameras, int cameraCount)
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

bool pluggedIn(const std::string& devicePath)
{
    auto devices = getDevices();
    for (const auto & device : devices)
    {
        if (device.path == devicePath)
            return true;
    }
    return false;
}

} // namespace detail
} // namespace audio
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif
