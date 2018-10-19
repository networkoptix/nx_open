#ifdef __linux__

#include "rpi_utils.h"

#include <vector>
#include <dirent.h>
#include <sys/stat.h>

#include <nx/utils/app_info.h>

namespace nx {
namespace usb_cam {
namespace device {
namespace rpi {

namespace {

static const std::string kRpiV4l2DevicePath = "/dev";

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

} //namespace


std::vector<std::string> getRpiDevicePaths()
{
    const auto isDeviceFile =
        [](const char *path)
        {
            struct stat buffer;
            stat(path, &buffer);
            return S_ISCHR(buffer.st_mode);
        };

    std::vector<std::string> devicePaths;

    DIR *directory = opendir(kRpiV4l2DevicePath.c_str());
    if (!directory)
        return devicePaths;

    struct dirent *directoryEntry;
    while ((directoryEntry = readdir(directory)) != NULL)
    {
        if (!strstr(directoryEntry->d_name, "video"))
            continue;

        std::string devVideo = kRpiV4l2DevicePath + "/" + directoryEntry->d_name;
        if (isDeviceFile(devVideo.c_str()))
            devicePaths.push_back(devVideo);
    }

    closedir(directory);
    return devicePaths;
}

int getMmalMaxBitrate()
{
    return kMmalMaxBitrate;
}

std::vector<device::ResolutionData> getMmalResolutionList()
{
    return kMmalResolutionList;
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
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif