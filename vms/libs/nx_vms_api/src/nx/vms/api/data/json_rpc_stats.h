// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/json_rpc/websocket_connection_stats.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/json/int64_as_number.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API JsonRpcMessages
{
    /**%apidoc:integer */
    nx::json::Int64AsNumber requests = 0;

    /**%apidoc:integer */
    nx::json::Int64AsNumber responses = 0;

    /**%apidoc:integer */
    nx::json::Int64AsNumber notifications = 0;

    static inline JsonRpcMessages fromMessages(
        const nx::json_rpc::WebSocketConnectionMessages& origin)
    {
        return {
            .requests = (int64_t) origin.requests,
            .responses = (int64_t) origin.responses,
            .notifications = (int64_t) origin.notifications};
    }
};
#define JsonRpcMessages_Fields (requests)(responses)(notifications)
NX_VMS_API_DECLARE_STRUCT_EX(JsonRpcMessages, (json))
NX_REFLECTION_INSTRUMENT(JsonRpcMessages, JsonRpcMessages_Fields)

struct NX_VMS_API JsonRpcConnectionInfo
{
    /**%apidoc:integer */
    nx::json::Int64AsNumber connectionId = 0;

    nx::Uuid userId;
    QString userName;
    QString userHost;
    QString userAgent;

    /**%apidoc `requests` and `notifications` are sent by a client, `responses` are sent by Server. */
    JsonRpcMessages in;

    /**%apidoc `requests` and `notifications` are sent by Server, `responses` are sent by a client. */
    JsonRpcMessages out;

    /**%apidoc:integer */
    nx::json::Int64AsNumber totalInB = 0;

    /**%apidoc:integer */
    nx::json::Int64AsNumber totalOutB = 0;

    std::vector<QString> subscriptions;
};
#define JsonRpcConnectionInfo_Fields (connectionId)(userId)(userName)(userHost)(userAgent) \
    (in)(out)(totalInB)(totalOutB)(subscriptions)
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
