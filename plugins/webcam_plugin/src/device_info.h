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

        void setResolutionList(const QList<QSize>& resolutionList);
        QList<QSize> resolutionList();

        QSize highestResolution();

    private:
        QString m_deviceName;
        QList<QSize> m_resolutionList;
        QSize m_highestResolution;
    };

}