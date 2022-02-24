// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <map>
#include <string>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/id_data.h>

namespace nx::vms::api {

struct NX_VMS_API DeviceReplacementRequest: IdData
{
    /**%apidoc Device id to replace with. Can be obtained from "id", "physicalId" or "logicalId"
     * field via  GET on /rest/v1/devices.
     */
    QnUuid replaceWithDeviceId;

    /**%apidoc[opt] If true, than only report is generated, no actual replacement done. */
    bool dryRun = false;
};

#define DeviceReplacementRequest_Fields \
    IdData_Fields \
    (replaceWithDeviceId) \
    (dryRun)

QN_FUSION_DECLARE_FUNCTIONS(DeviceReplacementRequest, (json), NX_VMS_API)

struct DeviceReplacementResponse
{
    /**%apidoc:object A list of (Sub task, issue) pairs. */
    std::map<std::string, std::string> report;

    /**%apidoc Indicates if it is possible to proceed with replacement. */
    bool compatible = false;
};

#define DeviceReplacementResponse_Fields (report)(compatible)

QN_FUSION_DECLARE_FUNCTIONS(DeviceReplacementResponse, (json), NX_VMS_API)

} // namespace nx::vms::api