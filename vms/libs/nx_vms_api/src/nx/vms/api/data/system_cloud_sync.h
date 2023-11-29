// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(SystemCloudService,

    /**%apidoc[unused] */
    all,

    /**%apidoc
     * Fetch information about Cloud users, System name, 2FA enabled.
     */
    cdb,

    /**%apidoc
     * Fetch information about Channel Partner, Organization, available services, send SaaS reports.
     */
    cps,

    /**%apidoc
     * Fetch information about the Cloud Storage services available.
     */
    css
);

struct NX_VMS_API SystemCloudSyncRequest
{
    SystemCloudService service = SystemCloudService::all;

    /**%apidoc[opt] Wait until the data is processed. */
    bool waitForDone = false;
};
#define SystemCloudSyncRequest_Fields (waitForDone)
NX_REFLECTION_INSTRUMENT(SystemCloudSyncRequest, SystemCloudSyncRequest_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SystemCloudSyncRequest, (json), NX_VMS_API)

struct NX_VMS_API CloudPullingStatus
{
    bool isRunning = false;
    std::optional<std::chrono::milliseconds> timeSinceLastSyncMs{};
    QString message;
};
#define CloudPullingStatus_Fields (isRunning)(timeSinceLastSyncMs)
QN_FUSION_DECLARE_FUNCTIONS(CloudPullingStatus, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(CloudPullingStatus, CloudPullingStatus_Fields)

} // namespace nx::vms::api
