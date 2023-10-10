// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <nx/vms/api/data/server_merge_data.h>

struct QnConfigureReply
{
    /**%apidoc Shows whether the Server must be restarted to apply settings. */
    bool restartNeeded = false;

    /**%apidoc List of unmerged servers. */
    std::vector<nx::vms::api::ServerMergeData> unmergedServers;
};
#define QnConfigureReply_Fields (restartNeeded)(unmergedServers)
QN_FUSION_DECLARE_FUNCTIONS(QnConfigureReply, (json), NX_VMS_COMMON_API)
