// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlapped_id_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(OverlappedIdResponse, (json),
    nx_vms_api_OverlappedIdResponse_Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SetOverlappedIdRequest, (json),
    nx_vms_api_SetOverlappedIdRequest_Fields);

} // namespace nx::vms::api
