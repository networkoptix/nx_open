// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API ServerInfoFilter
{
    /**%apidoc:string
     * Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`, or be "this" to
     * refer to the current Server.
     * %example this
     */
    QnUuid id;

    /**%apidoc[opt] Flag to filter out Server Info from offline Servers. Use this flag
     *     together with "_local" flag to control request propagation and response details.
     */
    bool onlyFreshInfo = true;
};
#define ServerInfoFilter_Fields (id)(onlyFreshInfo)
QN_FUSION_DECLARE_FUNCTIONS(ServerInfoFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerInfoFilter, ServerInfoFilter_Fields)

} // namespace nx::vms::api
