// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/id_data.h>

namespace nx::vms::api {

struct NX_VMS_API DeviceAdvancedFilter
{
    /**%apidoc:string
     * Device id to get Advanced Parameters from (can be obtained from "id", "physicalId" or
     * "logicalId" field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain
     * cameras).
     */
    nx::Uuid deviceId;

    /**%apidoc[opt] Parameter id to read. */
    QString id;

    const DeviceAdvancedFilter& getId() const { return *this; }
    bool operator==(const DeviceAdvancedFilter&) const = default;
    QString toString() const;
};
#define DeviceAdvancedFilter_Fields (deviceId)(id)
QN_FUSION_DECLARE_FUNCTIONS(DeviceAdvancedFilter, (json), NX_VMS_API)

} // namespace nx::vms::api
