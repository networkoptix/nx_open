#pragma once

#include <nx/utils/log/assert.h>
#include <utils/common/software_version.h>
#include <nx/update/info/os_version.h>

namespace nx {
namespace update {
namespace info {

// #TODO #akulikov remove this?
struct NX_UPDATE_API UpdateRequestData
{
    QString cloudHost;
    QString customization;
    QnSoftwareVersion currentNxVersion;
    OsVersion osVersion;
    const QnSoftwareVersion* targetVersion = nullptr;
    bool isClient = false;

    UpdateRequestData(
        const QString& cloudHost,
        const QString& customization,
        const QnSoftwareVersion& currentNxVersion,
        const OsVersion& osVersion,
        const QnSoftwareVersion* targetVersion,
        bool isClient)
        :
        cloudHost(cloudHost),
        customization(customization),
        currentNxVersion(currentNxVersion),
        osVersion(osVersion),
        targetVersion(targetVersion),
        isClient(isClient)
    {}

    UpdateRequestData() = default;

    QString toString() const
    {
        return QString::fromLatin1(
            "cloud host=%1, customization=%2, current nx version=%3, os version = %4").arg(
                cloudHost, customization, currentNxVersion.toString(), osVersion.serialize());
    }
};

} // namespace info
} // namespace update
} // namespace nx
