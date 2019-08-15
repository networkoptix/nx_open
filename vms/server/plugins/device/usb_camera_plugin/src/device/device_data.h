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
    std::string audioPath;

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

    bool operator==(const DeviceData& rhs) const
    {
        return name == rhs.name
            && path == rhs.path
            && uniqueId == rhs.uniqueId
            && audioPath == rhs.audioPath;
    }

    bool operator!=(const DeviceData& rhs) const
    {
        return !operator==(rhs);
    }

    std::string toString() const
    {
        return std::string("device: { name: ") + name +
            ", path: " + path +
            ", uid: " + uniqueId +
            ", audio: " + audioPath + " }";
    }
};

} // namespace device
} // namespace usb_cam
} // namespace nx
