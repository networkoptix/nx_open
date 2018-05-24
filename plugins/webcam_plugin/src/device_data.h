#pragma once

#include <vector>
#include <string>

#include <camera/camera_plugin_types.h>
#include "camera/camera_plugin.h"

namespace nx {
namespace webcam_plugin {
namespace utils {

struct ResolutionData
{
    //resolution
    nxcip::Resolution resolution;
    
    // bitrate in Kb/s
    uint32_t bitrate;
    
    //maximum frames per second
    float maxFps;

    bool operator==(const ResolutionData & rhs) const
    {
        return resolution.width == rhs.resolution.width
            && resolution.height == rhs.resolution.height
            && bitrate == rhs.bitrate
            && maxFps == rhs.maxFps;
    }
};

class DeviceData
{
public:
    void setDeviceName(const std::string& deviceName);
    std::string deviceName() const;

    void setDevicePath(const std::string& devicePath);
    std::string devicePath() const;

private:
    std::string m_deviceName;
    std::string m_devicePath;
};

} // namespace utils
} // namespace webcam_plugin
} // namespace nx
