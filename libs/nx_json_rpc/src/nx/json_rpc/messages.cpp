// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "messages.h"

#include <nx/fusion/model_functions.h>

namespace nx::json_rpc {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Request, (json), JsonRpcRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Error, (json), JsonRpcError_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Response, (json), JsonRpcResponse_Fields)

Response Response::makeError(ResponseId id, Error error)
{
    return {.id = id, .error = error};
}

} // namespace nx::json_rpc
