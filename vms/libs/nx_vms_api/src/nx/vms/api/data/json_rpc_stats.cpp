// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "json_rpc_stats.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcMessages, (json), JsonRpcMessages_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcConnectionInfo, (json), JsonRpcConnectionInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcStats, (json), JsonRpcStats_Fields)

} // namespace nx::vms::api
