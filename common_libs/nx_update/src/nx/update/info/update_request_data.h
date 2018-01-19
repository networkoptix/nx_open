#pragma once

#include <utils/common/software_version.h>

namespace nx {
namespace update {
namespace info {

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

    bool matches(const QString& target) const
    {
        return target.contains(family)
            && target.contains(architecture)
            && target.contains(version);
    }

    QString toString() const
    {
        return lit("%1.%2.%3").arg(family).arg(architecture).arg(version);
    }
};

OsVersion ubuntuX64();
OsVersion ubuntuX86();
OsVersion windowsX64();
OsVersion windowsX86();
OsVersion armBpi();
OsVersion armRpi();
OsVersion armBananapi();

struct UpdateRequestData
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

    QString toString() const
    {
        return lit("cloud host=%1, customization=%2, current nx version=%3")
            .arg(cloudHost)
            .arg(customization)
            .arg(currentNxVersion.toString());
    }
};

struct UpdateFileRequestData: UpdateRequestData
{
    OsVersion osVersion;

    UpdateFileRequestData(
        const QString& cloudHost,
        const QString& customization,
        const QnSoftwareVersion& currentNxVersion,
        const OsVersion& osVersion)
        :
        UpdateRequestData(cloudHost, customization, currentNxVersion),
        osVersion(osVersion)
    {}

    QString toString() const
    {
        return UpdateRequestData::toString() + lit(", os=%1").arg(osVersion.toString());
    }
};

} // namespace info
} // namespace update
} // namespace nx
