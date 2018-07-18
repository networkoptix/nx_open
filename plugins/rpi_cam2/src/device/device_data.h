#pragma once

#include <vector>
#include <string>

namespace nx {
namespace device {

struct ResolutionData
{
    // resolution
    int width;
    int height;
    
    // bitrate in Kb/s
    int bitrate;
    
    // maximum frames per second
    int maxFps;

    bool operator==(const ResolutionData & rhs) const
    {
        return width == rhs.width
            && height == rhs.height
            && bitrate == rhs.bitrate
            && maxFps == rhs.maxFps;
    }
};

struct DeviceData
{
    std::string deviceName;
    std::string devicePath;

    DeviceData(const std::string& deviceName, const std::string& devicePath)
    {
        this->deviceName = deviceName;
        this->devicePath = devicePath;
    }
};

} // namespace device
} // namespace nx
