#include "StdAfx.h"
#include "device_data.h"

namespace nx {
namespace webcam_plugin {
namespace utils {

void DeviceData::setDeviceName(const QString & deviceName)
{
    m_deviceName = deviceName;
}
QString DeviceData::deviceName()
{
    return m_deviceName;
}

void DeviceData::setDevicePath(const QString & devicePath)
{
    m_devicePath = devicePath;
}

QString DeviceData::devicePath()
{
    return m_devicePath;
}

void DeviceData::setResolutionList(const QList<ResolutionData>& resolutionDataList)
{
    m_resolutionDataList = resolutionDataList;
}

QList<ResolutionData> DeviceData::resolutionList()
{
    return m_resolutionDataList;
}

} // namespace utils
} // namespace webcam_plugin
} // namespace nx
