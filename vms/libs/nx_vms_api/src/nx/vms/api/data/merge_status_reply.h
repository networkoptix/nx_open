// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data_macros.h>

namespace nx::vms::api {

/**%apidoc Details about the last Merge operation. */
struct NX_VMS_API MergeStatusReply
{
    /**%apidoc Id of the last Merge operation. */
    QnUuid mergeId;

    /**%apidoc Whether the last Merge operation is in progress. */
    bool mergeInProgress = false;
};
#define MergeStatusReply_Fields (mergeId)(mergeInProgress)
NX_VMS_API_DECLARE_STRUCT(MergeStatusReply)

} // namespace nx::vms::api
