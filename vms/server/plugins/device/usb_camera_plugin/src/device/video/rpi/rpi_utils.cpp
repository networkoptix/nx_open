#ifdef __linux__

#include "rpi_utils.h"

#include <stdio.h>

#include <nx/utils/app_info.h>

namespace nx {
namespace usb_cam {
namespace device {
namespace video {
namespace rpi {

namespace {

static std::string kMmalUniqueId;
static bool kGotUniqueId = false;

// Maximum bits per second, taken from v4l2-ctl --list-ctrls.
static int constexpr kMmalMaxBitrate=10000000;

// The v4l2 driver for the integrated camera doesn't report resolutions accurately,
// so we have to hardcode it here
static const std::vector<ResolutionData> kMmalResolutionList = 
{
    {1920, 1080, 30},
    {1920, 1080, 25},
    {1920, 1080, 20},
    {1920, 1080, 15},
    {1920, 1080, 10},
    {1920, 1080, 5},
    {1280, 720, 30},
    {1280, 720, 25},
    {1280, 720, 20},
    {1280, 720, 15},
    {1280, 720, 10},
    {1280, 720, 5},
    {800, 600, 30},
    {800, 600, 25},
    {800, 600, 20},
    {800, 600, 15},
    {800, 600, 10},
    {800, 600, 5},
    {640, 480, 30},
    {640, 480, 25},
    {640, 480, 20},
    {640, 480, 15},
    {640, 480, 10},
    {640, 480, 5},
    {640, 360, 30},
    {640, 360, 25},
    {640, 360, 20},
    {640, 360, 15},
    {640, 360, 10},
    {640, 360, 5},
    {480, 270, 30},
    {480, 270, 25},
    {480, 270, 20},
    {480, 270, 15},
    {480, 270, 10},
    {480, 270, 5} 
};

static std::string getUniqueId()
{
    std::string serialNumber;

    FILE * file = fopen("/proc/cpuinfo", "r");
    if (!file)
        return std::string();

    // Not making this static because this function is only called once.
    constexpr char serialKey[] = "Serial";
    const std::string delimitter(": ");

    char * line = NULL;
    size_t length = 0;
    while (getline(&line, &length, file) != -1)
    {
        // Expecting to find a line of the form: "Serial		: 000000001de09d4f",
        // where "000000001de09d4f" is the serial number for the Raspberry Pi.
        if (!strstr(line, serialKey))
            continue;

        std::string keyValuePair(line);
        size_t delimitterPosition = keyValuePair.find(delimitter);
        if (delimitterPosition == std::string::npos)
            break;

        delimitterPosition += delimitter.size(); //< Advance past ": ".
        if (delimitterPosition >= keyValuePair.size())
            break;

        // Drop everything before ": " and "\n" at the end, if it has one.
        size_t newLinePosition = keyValuePair.find("\n");
        if (newLinePosition != std::string::npos)
        {
            serialNumber = 
                keyValuePair.substr(delimitterPosition, newLinePosition - delimitterPosition);
        }
        else
        {
            serialNumber = keyValuePair.substr(delimitterPosition);
        }

        break;
    }

    free(line);
    fclose(file);

    return serialNumber;
}

} //namespace

std::string getMmalUniqueId()
{
    if (!kGotUniqueId)
    {
        kGotUniqueId = true;
        kMmalUniqueId = getUniqueId();
    }
    return kMmalUniqueId;
}

std::vector<device::video::ResolutionData> getMmalResolutionList()
{
    return kMmalResolutionList;
}

int getMmalMaxBitrate()
{
    return kMmalMaxBitrate;
}

bool isMmalCamera(const std::string& deviceName)
{
    return deviceName.find("mmal") != std::string::npos;
}

bool isRpi()
{
    return nx::utils::AppInfo::isRaspberryPi();
}
    
} // namespace rpi
} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif // __linux__
