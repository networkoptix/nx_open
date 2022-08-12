// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "publication_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::update {

bool Package::isValid() const
{
    return !file.isEmpty()
        && component != Component::unknown
        && !platform.isEmpty()
        && !md5.isEmpty()
        && size > 0;
}

bool Package::isCompatibleTo(const utils::OsInfo& osInfo, bool ignoreVersion) const
{
    return isPackageCompatibleTo(platform, flavor, variants, osInfo, ignoreVersion);
}

bool Package::isNewerThan(const QString& variant, const Package& other) const
{
    return isPackageNewerForVariant(variant, variants, other.variants);
}

bool Package::isSameTarget(const Package& other) const
{
    return component == other.component
        && platform == other.platform
        && flavor == other.flavor
        && variants == other.variants;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PublicationInfo, (json), PublicationInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Package, (hash)(json), Package_Fields)

PublicationInfo::FindPackageResult PublicationInfo::findPackage(
    Component component, const utils::OsInfo& osInfo) const
{
    bool variantFound = false;
    const Package* bestPackage = nullptr;

    for (const auto& package: packages)
    {
        if (package.component != component)
            continue;

        if (package.isCompatibleTo(osInfo))
        {
            if (!bestPackage || package.isNewerThan(osInfo.variant, *bestPackage))
                bestPackage = &package;
        }
        else if (package.isCompatibleTo(osInfo, /*ignoreVersion*/ true))
        {
            variantFound = true;
        }
    }

    if (bestPackage)
        return *bestPackage;

    return variantFound ? FindPackageError::osVersionNotSupported : FindPackageError::notFound;
}

} // namespace nx::vms::update
