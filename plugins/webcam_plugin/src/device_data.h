#pragma once

#include <camera/camera_plugin_types.h>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSize>

namespace nx {
namespace webcam_plugin {
namespace utils {

struct ResolutionData
{
    QSize resolution;
    nxcip::CompressionType codecID;
    //nxcip::PixelFormat pixelFormat;

    bool operator==(const ResolutionData & rhs) const
    {
        return resolution == rhs.resolution
        && codecID == rhs.codecID;
    }
};

class DeviceData
{
public:
    void setDeviceName(const QString& deviceName);
    QString deviceName();

    void setDevicePath(const QString& devicePath);
    QString devicePath();

    void setResolutionList(const QList<ResolutionData>& resolutionDataList);
    QList<ResolutionData> resolutionList();

private:
    QString m_deviceName;
    QString m_devicePath;

    QList<ResolutionData> m_resolutionDataList;
};

} // namespace utils
} // namespace webcam_plugin
} // namespace nx
