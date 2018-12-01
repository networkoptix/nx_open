#pragma once

#include <string>

namespace nx {
namespace usb_cam {
namespace device {

struct DeviceData
{
    std::string name;
    std::string path;
    std::string uniqueId;

    DeviceData(
        const std::string& name,
        const std::string& path,
        const std::string& uniqueId)
        :
        name(name),
        path(path),
        uniqueId(uniqueId)
    {
    }
};

} // namespace device
} // namespace usb_cam
} // namespace nx
