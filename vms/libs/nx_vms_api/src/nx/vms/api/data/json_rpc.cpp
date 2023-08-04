// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "json_rpc.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcRequest, (json), JsonRpcRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcError, (json), JsonRpcError_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcResponse, (json), JsonRpcResponse_Fields)

JsonRpcResponse JsonRpcResponse::makeError(JsonRpcResponseId id, JsonRpcError error)
{
    return {.id = id, .error = error};
}

} // namespace nx::vms::api
