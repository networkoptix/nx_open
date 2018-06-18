#include "device_data.h"

namespace nx {
namespace webcam_plugin {
namespace utils {

void DeviceData::setDeviceName(const std::string & deviceName)
{
    m_deviceName = deviceName;
}
std::string DeviceData::deviceName() const
{
    return m_deviceName;
}

void DeviceData::setDevicePath(const std::string & devicePath)
{
    m_devicePath = devicePath;
}

std::string DeviceData::devicePath() const
{
    return m_devicePath;
}

} // namespace utils
} // namespace webcam_plugin
} // namespace nx
