// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "update_contents.h"

namespace nx::vms::client::desktop {

void UpdateContents::resetVerification()
{
    error = common::update::InformationError::noError;

    filesToUpload.clear();
    missingUpdate.clear();
    invalidVersion.clear();
    ignorePeers.clear();

    unsuportedSystemsReport.clear();
    peersWithUpdate.clear();
    manualPackages.clear();
    packageProperties.clear();
    packagesGenerated = false;
}

bool UpdateContents::peerHasUpdate(const nx::Uuid& id) const
{
    for (const auto& props: packageProperties)
    {
        if (props.targets.contains(id))
            return true;
    }
    return false;
}

uint64_t UpdateContents::getClientSpaceRequirements(bool withClient) const
{
    uint64_t spaceRequired = 0;
    for (const auto& package: manualPackages)
        spaceRequired += package.size;
    if (withClient && clientPackage)
        spaceRequired += clientPackage->size;
    return spaceRequired;
}

UpdateDeliveryInfo UpdateContents::getUpdateDeliveryInfo() const
{
    UpdateDeliveryInfo result;
    result.version = info.version;
    result.releaseDateMs = info.releaseDate;
    result.releaseDeliveryDays = info.releaseDeliveryDays;
    return result;
}

bool UpdateContents::isValidToInstall() const
{
    return missingUpdate.empty()
        && unsuportedSystemsReport.empty()
        && !info.version.isNull()
        && invalidVersion.empty()
        && error == nx::vms::common::update::InformationError::noError;
}

bool UpdateContents::isEmpty() const
{
    return info.packages.empty() || info.version.isNull();
}

bool UpdateContents::preferOtherUpdate(const UpdateContents& other) const
{
    if (isEmpty() && !other.isEmpty())
        return true;

    // Prefer update from mediaservers.
    if (sourceType != UpdateSourceType::mediaservers
        && other.sourceType == UpdateSourceType::mediaservers)
    {
        return true;
    }

    return other.info.version >= info.version;
}

} // namespace nx::vms::client::desktop
