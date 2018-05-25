#ifdef __linux__

#include "v4l2_utils.h"

#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <string>

namespace nx {
namespace webcam_plugin {
namespace utils {
namespace v4l2 {

std::vector<DeviceData> getDeviceList()
{
    auto isDeviceFile = [](const char *path)
    {
        struct stat buf;
        stat(path, &buf);
        return S_ISCHR(buf.st_mode);
    };

    std::vector<DeviceData> devices;

    std::string dev("/dev");
    DIR *dir = opendir(dev.c_str());
    if (!dir)
        return devices;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
    {
        char * video = strstr(ent->d_name, "video");
        if(!video)
            continue;

        std::string devVideo = dev + "/" + ent->d_name;
        if(isDeviceFile(devVideo.c_str()))
        {
            DeviceData d;
            d.setDeviceName(devVideo);
            d.setDevicePath(devVideo);
            devices.push_back(d);
        }
    }
    closedir(dir);
    return devices;
}

std::vector<nxcip::CompressionType> getSupportedCodecs(const char * devicePath)
{
    // todo
    return std::vector<nxcip::CompressionType>();
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID)
{
    // todo
    return std::vector<ResolutionData>();
}

} // namespace v4l2
} // namespace utils
} // namespace webcam_plugin
} // namespace nx

#endif