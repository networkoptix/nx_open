// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/merge_systems_common.h>

namespace nx::vms::api { struct ModuleInformation; }

namespace nx::vms::client::desktop {

/**
 * Implements system merge checks. Using following system merge terms:
 *
 * Proxy - the server to which the client is connected.
 * Target - foreign server, to merge to current system.
 */
class MergeSystemsValidator
{
public:
    MergeSystemsValidator() = delete;

    static MergeSystemsStatus checkVersionCompatibility(
        const nx::vms::api::ModuleInformation& targetInfo);

    static MergeSystemsStatus checkCloudCompatibility(
        QnMediaServerResourcePtr proxy,
        const nx::vms::api::ModuleInformation& targetInfo);
};

} // namespace nx::vms::client::desktop
