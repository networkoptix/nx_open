// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "merge_systems_validator.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/utils/software_version.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

namespace nx::vms::client::desktop {

MergeSystemsStatus MergeSystemsValidator::checkVersionCompatibility(
    const nx::vms::api::ModuleInformation& targetInfo)
{
    using nx::vms::common::ServerCompatibilityValidator;

    if (!ServerCompatibilityValidator::isCompatible(targetInfo,
        ServerCompatibilityValidator::Purpose::merge))
    {
        return MergeSystemsStatus::incompatibleVersion;
    }

    return MergeSystemsStatus::ok;
}

MergeSystemsStatus MergeSystemsValidator::checkCloudCompatibility(
    QnMediaServerResourcePtr proxy,
    const nx::vms::api::ModuleInformation& targetInfo)
{
    const bool proxyIsCloud = helpers::isCloudSystem(proxy->getModuleInformation());
    const bool targetIsCloud = helpers::isCloudSystem(targetInfo);

    if (proxyIsCloud && targetIsCloud)
        return MergeSystemsStatus::bothSystemBoundToCloud;
    if (targetIsCloud)
        return MergeSystemsStatus::dependentSystemBoundToCloud;

    return MergeSystemsStatus::ok;
}

} // namespace nx::vms::client::desktop
