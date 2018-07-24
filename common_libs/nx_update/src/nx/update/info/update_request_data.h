#pragma once

#include <nx/utils/log/assert.h>
#include <nx/update/info/os_version.h>
#include <nx/utils/software_version.h>

namespace nx {
namespace update {
namespace info {

// #TODO #akulikov remove this?
struct NX_UPDATE_API UpdateRequestData
{
    QString cloudHost;
    QString customization;
    nx::utils::SoftwareVersion currentNxVersion;
    OsVersion osVersion;
    const nx::utils::SoftwareVersion* targetVersion = nullptr;
    bool isClient = false;

    UpdateRequestData(
        const QString& cloudHost,
        const QString& customization,
        const nx::utils::SoftwareVersion& currentNxVersion,
        const OsVersion& osVersion,
        const nx::utils::SoftwareVersion* targetVersion,
        bool isClient)
        :
        cloudHost(cloudHost),
        customization(customization),
        currentNxVersion(currentNxVersion),
        osVersion(osVersion),
        targetVersion(targetVersion),
        isClient(isClient)
    {
    }

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
