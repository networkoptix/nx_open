// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

struct NX_VMS_API OverlappedIdResponse
{
    /**%apidoc
     * Current overlapped id used on the NVR. -1 means that the latest overlapped id is used.
     */
    int currentOverlappedId = -1;

    /**%apidoc
     * List of all available on the NVR overlapped ids.
     */
    std::vector<int> availableOverlappedIds;
};
#define nx_vms_api_OverlappedIdResponse_Fields (currentOverlappedId)(availableOverlappedIds)

QN_FUSION_DECLARE_FUNCTIONS(OverlappedIdResponse, (json), NX_VMS_API);

struct NX_VMS_API SetOverlappedIdRequest
{
    /**%apidoc
     * Group id of the NVR.
     */
    QString groupId;

    /**%apidoc
     * Desired overlapped id to be used on the NVR.
     */
    int overlappedId = -1;
};
#define nx_vms_api_SetOverlappedIdRequest_Fields (groupId)(overlappedId)

QN_FUSION_DECLARE_FUNCTIONS(SetOverlappedIdRequest, (json), NX_VMS_API);

} // namespace nx::vms::api
