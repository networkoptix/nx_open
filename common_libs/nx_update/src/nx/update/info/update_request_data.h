#pragma once

#include <nx/utils/log/assert.h>
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

    QString toString() const
    {
        return lit("%1.%2.%3").arg(family).arg(architecture).arg(version);
    }

    static OsVersion fromString(const QString& s)
    {
        auto splits = s.split('.');
        NX_ASSERT(splits.size() == 3);
        return OsVersion(splits[0], splits[1], splits[2]);
    }
};

NX_UPDATE_API bool operator==(const OsVersion& lhs, const OsVersion& rhs);
NX_UPDATE_API uint qHash(const OsVersion& osVersion);

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
        return lit("cloud host=%1, customization=%2, current nx version=%3")
            .arg(cloudHost)
            .arg(customization)
            .arg(currentNxVersion.toString());
    }
};

struct NX_UPDATE_API UpdateFileRequestData: UpdateRequestData
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

    UpdateFileRequestData() = default;

    QString toString() const
    {
        return UpdateRequestData::toString() + lit(", os=%1").arg(osVersion.toString());
    }
};

NX_UPDATE_API bool operator==(const UpdateFileRequestData& lhs, const UpdateFileRequestData& rhs);
NX_UPDATE_API uint qHash(const UpdateFileRequestData& fileRequestData);

} // namespace info
} // namespace update
} // namespace nx
