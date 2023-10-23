// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/storage_location.h>

namespace nx::vms::api {

struct NX_VMS_API RebuildArchiveFilter
{
    /**%apidoc:string
     * Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or be "this" to
     * refer to the current Server.
     * %example this
     */
    QnUuid id;

    /**%apidoc[opt] Storage location. */
    StorageLocation location = StorageLocation::both;

    /**%apidoc Starting time point of the rebuild process, in milliseconds. */
    std::optional<std::chrono::system_clock::time_point> startPointMs;
};

#define RebuildArchiveFilter_Fields (id)(location)(startPointMs)

QN_FUSION_DECLARE_FUNCTIONS(RebuildArchiveFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(RebuildArchiveFilter, RebuildArchiveFilter_Fields)

} // namespace nx::vms::api
