// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/json/value_or_array.h>

namespace nx::vms::api {

struct NX_VMS_API ServerValueOrArrayFilter
{
    /**%apidoc[opt]:stringArray
     * Server id(s). Can be obtained from "id" field via `GET /rest/v{1-}/servers`.
     */
    nx::vms::api::json::ValueOrArray<nx::Uuid> id;
};
#define ServerValueOrArrayFilter_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(ServerValueOrArrayFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerValueOrArrayFilter, ServerValueOrArrayFilter_Fields)

} // namespace nx::vms::api
