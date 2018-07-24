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
    
    // maximum frames per second
    int maxFps;

    ResolutionData():
        width(0),
        height(0),
        maxFps(0)
    {
    }

    ResolutionData(int width, int height, int maxFps):
        width(width),
        height(height),
        maxFps(maxFps)
        {
        }

    bool operator==(const ResolutionData & rhs) const
    {
        return width == rhs.width
            && height == rhs.height
            && maxFps == rhs.maxFps;
    }
};

struct DeviceData
{
    std::string deviceName;
    std::string devicePath;

    DeviceData(const std::string& deviceName, const std::string& devicePath):
        deviceName(deviceName),
        devicePath(devicePath)
    {
    }
};

} // namespace device
} // namespace nx
