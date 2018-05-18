#pragma once

#include <camera/camera_plugin_types.h>

#include <vector>
#include <string>
#include "camera/camera_plugin.h"

namespace nx {
namespace webcam_plugin {
namespace utils {

struct ResolutionData
{
    nxcip::Resolution resolution;
    nxcip::CompressionType codecID;
    //nxcip::PixelFormat pixelFormat;

    bool operator==(const ResolutionData & rhs) const
    {
        return resolution.width == rhs.resolution.width
            && resolution.height == rhs.resolution.height
            && codecID == rhs.codecID;
    }
};

class DeviceData
{
public:
    void setDeviceName(const std::string& deviceName);
    std::string deviceName() const;

    void setDevicePath(const std::string& devicePath);
    std::string devicePath() const;

    void setResolutionList(const std::vector<ResolutionData>& resolutionDataList);
    std::vector<ResolutionData> resolutionList();

private:
    std::string m_deviceName;
    std::string m_devicePath;

    std::vector<ResolutionData> m_resolutionDataList;
};

} // namespace utils
} // namespace webcam_plugin
} // namespace nx
