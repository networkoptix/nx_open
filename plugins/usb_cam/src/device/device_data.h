#pragma once

#include <vector>
#include <string>

#include <camera/camera_plugin_types.h>

#include "abstract_compression_type_descriptor.h"

namespace nx {
namespace device {

struct ResolutionData
{
    // resolution
    int width;
    int height;
    
    // maximum frames per second
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
