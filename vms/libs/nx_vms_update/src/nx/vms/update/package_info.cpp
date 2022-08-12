// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "package_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::update {

using nx::vms::api::SoftwareVersion;

bool PackageInfo::isValid() const
{
    return !version.isNull()
        && component != Component::unknown
        && !platform.isEmpty()
        && !installScript.isEmpty();
}

bool PackageInfo::isCompatibleTo(const utils::OsInfo& osInfo, bool ignoreVersion) const
{
    return isPackageCompatibleTo(platform, flavor, variants, osInfo, ignoreVersion);
}

bool isPackageCompatibleTo(
    const QString& packagePlatform,
    const QString& packageFlavor,
    const PlatformVariantList& packageVariants,
    const nx::utils::OsInfo& osInfo,
    bool ignoreVersion)
{
    if (packageFlavor != osInfo.flavor)
        return false;

    if (packagePlatform != osInfo.platform)
    {
        if (packagePlatform == "macos" && osInfo.platform == "macos_x64")
        {
            // Since 5.0 we support macOS ARM64. Because of that we renamed `macos` platform to
            // `macos_x64`. Here we have to take this renaming into account. We accept `macos`
            // package for `macos_x64` platform.

            // Do nothing here, just keep going.
        }
        else
        {
            return false;
        }
    }

    if (packageVariants.isEmpty())
        return true;

    const SoftwareVersion currentVersion(osInfo.variantVersion);

    for (const PlatformVariant& variant: packageVariants)
    {
        if (variant.name != osInfo.variant)
            continue;

        if (ignoreVersion)
            return true;

        if (!variant.minimumVersion.isEmpty()
            && SoftwareVersion(variant.minimumVersion) > currentVersion)
        {
            return false;
        }
        if (!variant.maximumVersion.isEmpty()
            && SoftwareVersion(variant.maximumVersion) < currentVersion)
        {
            return false;
        }

        return true;
    }

    return false;
}

bool isPackageNewerForVariant(
    const QString& variant,
    const PlatformVariantList& packageVariants,
    const PlatformVariantList& otherPackageVariants)
{
    const auto it = std::find_if(packageVariants.begin(), packageVariants.end(),
        [&variant](const PlatformVariant& v) { return v.name == variant; });
    if (it == packageVariants.end())
        return false;

    const auto itOther = std::find_if(otherPackageVariants.begin(), otherPackageVariants.end(),
        [&variant](const PlatformVariant& v) { return v.name == variant; });
    if (itOther == otherPackageVariants.end())
        return true;

    return SoftwareVersion(it->minimumVersion) >= SoftwareVersion(itOther->minimumVersion);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PlatformVariant, (hash)(json), PlatformVariant_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PackageInfo, (json), PackageInfo_Fields)

} // namespace nx::vms::update
