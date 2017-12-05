#pragma once

#include <utils/common/software_version.h>

namespace nx {
namespace update {
namespace info {

// todo: introduce some kind of constructor with predefined constants
struct OsVersion
{
    QString family;
    QString architecture;
    QString version;

    OsVersion(const QString& family, const QString& architecture, const QString& version) :
        family(family),
        architecture(architecture),
        version(version)
    {}

    OsVersion() = default;
    OsVersion(const OsVersion&) = default;
    OsVersion& operator = (const OsVersion&) = default;

    bool isEmpty() const
    {
        return family.isEmpty() && architecture.isEmpty() && version.isEmpty();
    }
};

struct UpdateRequestData
{
    QString cloudHost;
    QString customization;
    QnSoftwareVersion currentNxVersion;
    OsVersion osVersion; // todo: currently not used, use it! https://networkoptix.atlassian.net/browse/VMS-7134

    UpdateRequestData(
        const QString& cloudHost,
        const QString& customization,
        const QnSoftwareVersion& currentNxVersion,
        const OsVersion& osVersion = OsVersion())
        :
        cloudHost(cloudHost),
        customization(customization),
        currentNxVersion(currentNxVersion),
        osVersion(osVersion)
    {}
};

} // namespace info
} // namespace update
} // namespace nx
