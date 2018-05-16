#include "StdAfx.h"
#include "device_info.h"

namespace utils{
    void DeviceInfo::setDeviceName(const QString & deviceName)
    {
        m_deviceName = deviceName;
    }
    QString DeviceInfo::deviceName()
    {
        return m_deviceName;
    }

    void DeviceInfo::setDevicePath(const QString & devicePath)
    {
        m_devicePath = devicePath;
    }

    QString DeviceInfo::devicePath()
    {
        return m_devicePath;
    }

    void DeviceInfo::setResolutionList(const QList<QSize>& resolutionList)
    {
        m_resolutionList = resolutionList;
        m_highestResolution = QSize();
    }

    QList<QSize> DeviceInfo::resolutionList()
    {
        return m_resolutionList;
    }
    QSize DeviceInfo::highestResolution()
    {
        if (m_highestResolution != QSize())
            return m_highestResolution;

        if (m_resolutionList.empty())
            return QSize();

        m_highestResolution = m_resolutionList[0];
        for (const auto& size : m_resolutionList)
        {
            if (size.width() > m_highestResolution.width()
                && size.height() > m_highestResolution.height())
            {
                m_highestResolution = size;
            }
        }

        return m_highestResolution;
    }

} // namespace utils
