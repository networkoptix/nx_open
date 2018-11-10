#pragma once

#include <string>

namespace nx {
namespace usb_cam {
namespace device {

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
