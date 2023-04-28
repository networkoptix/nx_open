// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/reflect/instrument.h>
#include <nx/vms/api/data/software_version_serialization.h>

namespace nx::vms::api {

struct ClientUpdateSettings
{
    bool showFeatureInformer = true;
    bool enabled = true;
    std::chrono::milliseconds updateEnabledTimestampMs{0};

    nx::utils::SoftwareVersion pendingVersion;
    std::chrono::milliseconds plannedInstallationDateMs{0};

    bool operator==(const ClientUpdateSettings& other) const = default;
};

NX_REFLECTION_INSTRUMENT(ClientUpdateSettings, (showFeatureInformer)(enabled)
    (updateEnabledTimestampMs)(pendingVersion)(plannedInstallationDateMs));

} // namespace nx::vms::api
