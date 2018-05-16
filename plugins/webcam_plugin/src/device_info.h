#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSize>

namespace utils {

    class DeviceInfo
    {
    public:
        void setDeviceName(const QString& deviceName);
        QString deviceName();

        void setDevicePath(const QString& devicePath);
        QString devicePath();

        void setResolutionList(const QList<QSize>& resolutionList);
        QList<QSize> resolutionList();

        QSize highestResolution();

    private:
        QString m_deviceName;
        QString m_devicePath;
        QList<QSize> m_resolutionList;
        QSize m_highestResolution;
    };

}