#ifdef __linux__
#include "alsa_utils.h"

#include <limits.h>
#include <algorithm>
#include <map>
#include <vector>

#include <alsa/asoundlib.h>

#include <nx/utils/log/log.h>

namespace nx::usb_cam::device::audio::detail {

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

    bool isCameraAudioInput(const DeviceData& device) const
    {
        std::string modelName(device.name);
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
                for (const auto & motherBoardAudio : kMotherBoardAudioList)
                    isMotherBoardAudio = device.name.find(motherBoardAudio) != std::string::npos;

                auto it = std::find(motherBoardDevices.begin(), motherBoardDevices.end(), device);
                if (isMotherBoardAudio && it == motherBoardDevices.end())
                {
                    motherBoardDevices.push_back(device);
                }
                else if (std::find(devices.begin(), devices.end(), device) == devices.end())
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
    for (const auto motherBoardDevice : motherBoardDevices)
        devices.push_back(motherBoardDevice);

    return devices;
}

} // namespace

void selectAudioDevices(std::vector<DeviceData>& devices)
{
    std::vector<DeviceDescriptor> audioDevices = getDevices();
    std::map<DeviceDescriptor*, bool> audioTaken;
    std::vector<DeviceDescriptor*> defaults;

    for (int i = 0; i < (int)audioDevices.size(); ++i)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Found audio device, name [%1], path: %2",
            audioDevices[i].name, audioDevices[i].path);

        audioTaken.insert(std::pair<DeviceDescriptor*, bool>(&audioDevices[i], false));
        if ((audioDevices[i].isDefault || audioDevices[i].sysDefault) && audioDevices[i].isInput())
            defaults.push_back(&audioDevices[i]);
    }

    for (auto& device: devices)
    {
        bool mute = true;
        for (int j = 0; j < (int)audioDevices.size(); ++j)
        {
            DeviceDescriptor* audioDevice = &audioDevices[j];
            if (!audioTaken[audioDevice] && audioDevice->isCameraAudioInput(device) && audioDevice->sysDefault)
            {
                mute = false;
                audioTaken[audioDevice] = true;
                device.audioPath = audioDevice->path;
                break;
            }
        }
        if (mute && !defaults.empty())
            device.audioPath = defaults[0]->path;

        NX_DEBUG(NX_SCOPE_TAG, "Selected audio device: [%1]", device.toString());
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

void uninitialize()
{
    snd_config_update_free_global();
}

} // namespace nx::usb_cam::device::audio::detail

#endif
