// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "id_data.h"
#include "data_macros.h"

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
#define OverlappedIdResponse_Fields (currentOverlappedId)(availableOverlappedIds)
NX_VMS_API_DECLARE_STRUCT_EX(OverlappedIdResponse, (json))
NX_REFLECTION_INSTRUMENT(OverlappedIdResponse, OverlappedIdResponse_Fields)

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
#define SetOverlappedIdRequest_Fields (groupId)(overlappedId)

NX_VMS_API_DECLARE_STRUCT_EX(SetOverlappedIdRequest, (json))
NX_REFLECTION_INSTRUMENT(SetOverlappedIdRequest, SetOverlappedIdRequest_Fields)

struct NX_VMS_API OverlappedIdsRequest: IdData
{
    /**%apidoc
     * Group id of the NVR.
     */
    QString groupId;

    /**%apidoc
     * Desired overlapped id to be used on the NVR.
     */
    std::optional<int> overlappedId;

    OverlappedIdsRequest getId() const { return *this; }
    operator QString() const { return id.toString() + '.' + groupId; }
};
#define OverlappedIdsRequest_Fields (id)(groupId)(overlappedId)

NX_VMS_API_DECLARE_STRUCT_EX(OverlappedIdsRequest, (json))
NX_REFLECTION_INSTRUMENT(OverlappedIdsRequest, OverlappedIdsRequest_Fields)

} // namespace nx::vms::api
