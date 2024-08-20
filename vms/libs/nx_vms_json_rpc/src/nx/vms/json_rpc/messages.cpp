// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "messages.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::json_rpc {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcRequest, (json), JsonRpcRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcError, (json), JsonRpcError_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcResponse, (json), JsonRpcResponse_Fields)

JsonRpcResponse JsonRpcResponse::makeError(JsonRpcResponseId id, JsonRpcError error)
{
    return {.id = id, .error = error};
}

} // namespace nx::vms::json_rpc
