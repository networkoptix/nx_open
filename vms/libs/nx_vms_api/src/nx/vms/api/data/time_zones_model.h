// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

struct NX_VMS_API TimeZonesData
{
    /**%apidoc Time zone identifier. */
    QString id;

    /**%apidoc Time zone offset from UTC (in seconds). */
    std::chrono::seconds offsetFromUtcS{0};

    /**%apidoc Time zone verbose name in English. */
    QString displayName;

    /**%apidoc Whether the time zone has the DST feature. */
    bool hasDaylightTime = false;

    /**%apidoc
     * Whether the time zone is on DST right now. To be reported properly, the server machine
     * should have the correct current time set.
     */
    bool isDaylightTime = false;

    /**%apidoc Time zone description in English. */
    QString comment;
};
#define TimeZonesData_Fields \
    (id) \
    (offsetFromUtcS) \
    (displayName) \
    (hasDaylightTime) \
    (isDaylightTime) \
    (comment)
QN_FUSION_DECLARE_FUNCTIONS(TimeZonesData, (json), NX_VMS_API)

} // namespace nx::vms::api
