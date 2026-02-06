// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "json_rpc_stats.h"

#include <nx/fusion/model_functions.h>

namespace nx::json_rpc {

void serialize(QnJsonContext*, const NumericId& value, QJsonValue* out)
{
    *out = QJsonValue((double) value.value);
}

bool deserialize(QnJsonContext*, const QJsonValue& value, NumericId* out)
{
    if (!value.isDouble())
        return false;

    const double doubleValue = value.toDouble();
    const int64_t intValue = (int64_t) doubleValue;
    if ((double) intValue != doubleValue)
        return false;

    out->value = intValue;
    return true;
}

} // namespace nx::json_rpc

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcConnectionInfo, (json), JsonRpcConnectionInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonRpcStats, (json), JsonRpcStats_Fields)

} // namespace nx::vms::api
