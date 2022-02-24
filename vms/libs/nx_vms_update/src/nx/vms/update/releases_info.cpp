// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "releases_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::update {

namespace {

bool publicationTypeAllowed(PublicationType currentType, PublicationType type)
{
    switch (currentType)
    {
        case PublicationType::release:
            return type == PublicationType::release;

        case PublicationType::rc:
            return type == PublicationType::release || type == PublicationType::rc;

        case PublicationType::beta:
            return type == PublicationType::release
                || type == PublicationType::rc
                || type == PublicationType::beta;

        default:
            return false;
    }
}

bool isBuildPublished(const ReleaseInfo& releaseInfo)
{
    return releaseInfo.release_date > 0 && releaseInfo.release_delivery_days >= 0;
}

// Builds with unspecified release date or release delivery days are considered as not yet
// published and should be ignored. Exceptions are beta and rc publication types if the
// current build type and version is appropriate.
bool canReceiveUnpublishedBuild(
    const ReleaseInfo& releaseInfo,
    const nx::utils::SoftwareVersion& currentVersion)
{
    bool canPublicationTypeReceiveUnpublishedBuild =
        nx::build_info::publicationType() == PublicationType::beta
        || nx::build_info::publicationType() == PublicationType::rc;

    bool isUnpublishedVersionAppropriate =
        releaseInfo.version.major() == currentVersion.major()
        && releaseInfo.version.minor() == currentVersion.minor()
        && releaseInfo.version > currentVersion;

    return canPublicationTypeReceiveUnpublishedBuild && isUnpublishedVersionAppropriate;
}

} // namespace

std::optional<ReleaseInfo> ReleasesInfo::selectVmsRelease(
    const nx::utils::SoftwareVersion& currentVersion) const
{
    const ReleaseInfo* bestRelease = nullptr;

    for (const auto& release: releases)
    {
        if (release.product == Product::vms
            && release.publication_type == PublicationType::release
            && release.version >= currentVersion
            && isBuildPublished(release))
        {
            if (!bestRelease || bestRelease->version < release.version)
                bestRelease = &release;
        }
    }

    if (bestRelease)
        return *bestRelease;

    return {};
}

std::optional<ReleaseInfo> ReleasesInfo::selectDesktopClientRelease(
    const nx::utils::SoftwareVersion& currentVersion,
    const PublicationType publicationType,
    const int protocolVersion) const
{
    const ReleaseInfo* bestRelease = nullptr;

    for (const auto& release: releases)
    {
        if (!isBuildPublished(release) && !canReceiveUnpublishedBuild(release, currentVersion))
            continue;

        if ((release.product == Product::vms || release.product == Product::desktop_client)
            && publicationTypeAllowed(publicationType, release.publication_type)
            && release.protocol_version == protocolVersion
            && release.version >= currentVersion)
        {
            if (!bestRelease || bestRelease->version < release.version)
                bestRelease = &release;
        }
    }

    if (bestRelease)
        return *bestRelease;

    return {};
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ReleaseInfo, (json), ReleaseInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ReleasesInfo, (json), ReleasesInfo_Fields)

} // namespace nx::vms::update
