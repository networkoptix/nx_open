// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"
#include "server_merge_data.h"

namespace nx::vms::api {

/**%apidoc Details about the last Merge operation. */
struct NX_VMS_API MergeStatusReply
{
    /**%apidoc Id of the last Merge operation. */
    nx::Uuid mergeId;

    /**%apidoc Whether the last Merge operation is in progress. */
    bool mergeInProgress = false;

    /**%apidoc List of unmerged servers. */
    std::vector<ServerMergeData> unmergedServers;
    std::vector<std::string> warnings;
};
#define MergeStatusReply_Fields (mergeId)(mergeInProgress)(unmergedServers)(warnings)
NX_VMS_API_DECLARE_STRUCT(MergeStatusReply)
NX_REFLECTION_INSTRUMENT(MergeStatusReply, MergeStatusReply_Fields)

} // namespace nx::vms::api
