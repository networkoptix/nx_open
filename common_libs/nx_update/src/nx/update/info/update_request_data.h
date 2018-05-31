#pragma once

#include <nx/utils/log/assert.h>
#include <utils/common/software_version.h>
#include <nx/update/info/os_version.h>

namespace nx {
namespace update {
namespace info {

struct NX_UPDATE_API UpdateRequestData
{
    QString cloudHost;
    QString customization;
    QnSoftwareVersion currentNxVersion;

    UpdateRequestData(
        const QString& cloudHost,
        const QString& customization,
        const QnSoftwareVersion& currentNxVersion)
        :
        cloudHost(cloudHost),
        customization(customization),
        currentNxVersion(currentNxVersion)
    {}

    UpdateRequestData(): currentNxVersion("0.0.0.0") {}

    QString toString() const
    {
        return QString::fromLatin1("cloud host=%1, customization=%2, current nx version=%3").arg(
            cloudHost, customization, currentNxVersion.toString());
    }
};

struct NX_UPDATE_API UpdateFileRequestData: UpdateRequestData
{
    OsVersion osVersion;
    bool isClient = false;

    UpdateFileRequestData(
        const QString& cloudHost,
        const QString& customization,
        const QnSoftwareVersion& currentNxVersion,
        const OsVersion& osVersion,
        bool isClient)
        :
        UpdateRequestData(cloudHost, customization, currentNxVersion),
        osVersion(osVersion),
        isClient(isClient)
    {}

    UpdateFileRequestData() = default;

    QString toString() const
    {
        return UpdateRequestData::toString()
            + QString::fromLatin1(", os=%1").arg(osVersion.serialize());
    }
};

} // namespace info
} // namespace update
} // namespace nx
