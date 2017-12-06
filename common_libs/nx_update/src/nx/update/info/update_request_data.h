#pragma once

#include <utils/common/software_version.h>

namespace nx {
namespace update {
namespace info {

struct NX_UPDATE_API OsVersion
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

    bool matches(const QString& target) const
    {
        return target.contains(family)
            && target.contains(architecture)
            && target.contains(version);
    }
};

NX_UPDATE_API OsVersion ubuntuX64();
NX_UPDATE_API OsVersion ubuntuX86();
NX_UPDATE_API OsVersion windowsX64();
NX_UPDATE_API OsVersion windowsX86();
NX_UPDATE_API OsVersion armBpi();
NX_UPDATE_API OsVersion armRpi();
NX_UPDATE_API OsVersion armBananapi();

struct NX_UPDATE_API UpdateRequestData
{
    QString cloudHost;
    QString customization;
    QnSoftwareVersion currentNxVersion;
    OsVersion osVersion;

    UpdateRequestData(
        const QString& cloudHost,
        const QString& customization,
        const QnSoftwareVersion& currentNxVersion,
        const OsVersion& osVersion)
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
