// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/api/data/software_version_serialization.h>

namespace nx::vms::api {

struct NX_VMS_API ClientUpdateSettings
{
    bool showFeatureInformer = true;
    bool enabled = true;

    /**%apidoc[opt]
     * Datetime since when the Client updates are allowed to be installed, in milliseconds since
     * epoch. Client updates released before this timestamp are ignored.
     */
    std::chrono::milliseconds updateEnabledTimestampMs{0};

    nx::utils::SoftwareVersion pendingVersion; /**<%apidoc:string */

    /**%apidoc[opt]
     * Planned Client update installation datetime, in milliseconds since epoch. Update is started
     * when this value becomes less than current time.
     */
    std::chrono::milliseconds plannedInstallationDateMs{0};

    ClientUpdateSettings();
    bool operator==(const ClientUpdateSettings& other) const = default;
    QByteArray toString() const;
};

#define ClientUpdateSettings_Fields \
    (showFeatureInformer)(enabled)(updateEnabledTimestampMs)(pendingVersion) \
    (plannedInstallationDateMs)

NX_REFLECTION_INSTRUMENT(ClientUpdateSettings, ClientUpdateSettings_Fields);
NX_REFLECTION_TAG_TYPE(ClientUpdateSettings, jsonSerializeChronoDurationAsNumber)
QN_FUSION_DECLARE_FUNCTIONS(ClientUpdateSettings, (json), NX_VMS_API)

} // namespace nx::vms::api
