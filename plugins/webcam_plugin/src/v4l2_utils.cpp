#ifdef __linux__

#include "v4l2_utils.h"

#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <string>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

namespace nx {
namespace webcam_plugin {
namespace utils {
namespace v4l2 {
   
std::vector<std::string> getDevicePaths();
std::string getDeviceName(int fileDescriptor);
    
std::string getDeviceName(int fileDescriptor)
{
    struct v4l2_capability deviceCapability;
    ioctl(fileDescriptor, VIDIOC_QUERYCAP, &deviceCapability);
    return std::string(reinterpret_cast<char*>(deviceCapability.card));
}

std::vector<std::string> getDevicePaths()
{
    auto isDeviceFile = [](const char *path)
    {
        struct stat buf;
        stat(path, &buf);
        return S_ISCHR(buf.st_mode);
    };

    std::vector<std::string> devices;

    std::string dev("/dev");
    DIR *dir = opendir(dev.c_str());
    if (!dir)
        return devices;
    
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
    {
        char * video = strstr(ent->d_name, "video");
        if (!video)
            continue;
        
        std::string devVideo = dev + "/" + ent->d_name;
        if(isDeviceFile(devVideo.c_str()))
            devices.push_back(devVideo);
    }
    
    closedir(dir);
    return devices;
}

std::vector<DeviceData> getDeviceList()
{
    std::vector<std::string> devicePaths = getDevicePaths();
    std::vector<DeviceData> deviceList;
    if(devicePaths.empty())
        return deviceList;
    
    for(const auto& devicePath : devicePaths)
    {
        int fileDescriptor = open(devicePath.c_str(), O_RDONLY);
        if(fileDescriptor == -1)
            continue;
        
        DeviceData d;
        d.setDeviceName(getDeviceName(fileDescriptor));
        d.setDevicePath(devicePath);
        deviceList.push_back(d);
        
        close(fileDescriptor);
    }
    
    return deviceList;
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