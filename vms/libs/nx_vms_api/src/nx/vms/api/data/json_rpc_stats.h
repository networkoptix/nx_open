// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/json_rpc/messages.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx::json_rpc {

NX_VMS_API void serialize(QnJsonContext* context, const NumericId& value, QJsonValue* out);
NX_VMS_API bool deserialize(QnJsonContext* context, const QJsonValue& value, NumericId* out);

} // namespace nx::json_rpc

namespace nx::vms::api {

struct NX_VMS_API JsonRpcConnectionInfo
{
    /**%apidoc:integer */
    nx::json_rpc::NumericId connectionId = 0;

    nx::Uuid userId;
    QString userName;
    QString userHost;
    QString userAgent;

    /**%apidoc:integer */
    nx::json_rpc::NumericId inRequests = 0;

    /**%apidoc:integer */
    nx::json_rpc::NumericId outRequests = 0;

    /**%apidoc:integer */
    nx::json_rpc::NumericId totalInB = 0;

    /**%apidoc:integer */
    nx::json_rpc::NumericId totalOutB = 0;

    std::vector<QString> subscriptions;
};
#define JsonRpcConnectionInfo_Fields (connectionId)(userId)(userName)(userHost)(userAgent) \
    (inRequests)(outRequests)(totalInB)(totalOutB)(subscriptions)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(JsonRpcConnectionInfo, (json))
NX_REFLECTION_INSTRUMENT(JsonRpcConnectionInfo, JsonRpcConnectionInfo_Fields)

struct NX_VMS_API JsonRpcStats
{
    nx::Uuid serverId;
    JsonRpcConnectionInfoList connections;
};
#define JsonRpcStats_Fields (serverId)(connections)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(JsonRpcStats, (json))
NX_REFLECTION_INSTRUMENT(JsonRpcStats, JsonRpcStats_Fields)

} // namespace nx::vms::api
