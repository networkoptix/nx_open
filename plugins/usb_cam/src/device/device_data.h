#pragma once

#include <vector>
#include <string>

namespace nx {
namespace usb_cam {
namespace device {

struct ResolutionData
{
    // Resolution
    int width;
    int height;
    
    // Maximum frames per second
    float fps;

    ResolutionData():
        width(0),
        height(0),
        fps(0)
    {
    }

    ResolutionData(int width, int height, float fps):
        width(width),
        height(height),
        fps(fps)
    {
    }

    bool operator==(const ResolutionData & rhs) const
    {
        return width == rhs.width
            && height == rhs.height
            && fps == rhs.fps;
    }

    float aspectRatio() const
    {
        return (float) width / height;
    }
};

struct DeviceData
{
    std::string deviceName;
    std::string devicePath;
    std::string uniqueId;

    DeviceData(
        const std::string& deviceName,
        const std::string& devicePath,
        const std::string& uniqueId)
        :
        deviceName(deviceName),
        devicePath(devicePath),
        uniqueId(uniqueId)
    {
    }
};

} // namespace device
} // namespace usb_cam
} // namespace nx
