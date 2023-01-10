// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/data/software_version.h>

namespace nx::vms::common::api {

struct NX_VMS_COMMON_API ClientUpdateSettings
{
    bool showFeatureInformer = true;
    bool enabled = true;
    std::chrono::milliseconds updateEnabledTimestamp{0};

    nx::vms::api::SoftwareVersion pendingVersion;
    std::chrono::milliseconds plannedInstallationDate{0};

    bool operator==(const ClientUpdateSettings& other) const = default;

    ClientUpdateSettings();
};

#define ClientUpdateSettings_Fields \
    (showFeatureInformer)(enabled)(updateEnabledTimestamp)(pendingVersion)(plannedInstallationDate)

QN_FUSION_DECLARE_FUNCTIONS(ClientUpdateSettings, (json), NX_VMS_COMMON_API)

} // namespace nx::vms::common::update
