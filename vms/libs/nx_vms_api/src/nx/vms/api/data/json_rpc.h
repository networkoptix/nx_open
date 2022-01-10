// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::api {

struct NX_VMS_API JsonRpcRequest
{
    /*const*/ std::string jsonrpc = "2.0";
    std::string method;

    /*
     * TODO: Replace with std::optional<std::variant<QJsonObject, QJsonArray>> when apidoctool will
     * support this.
     */
    /**%apidoc:any */
    std::optional<QJsonValue> params;

    /*
     * TODO: Replace with std::optional<std::variant<int, std::string>> when apidoctool will
     * support std::variant.
     */
    /**%apidoc:any */
    std::optional<QJsonValue> id;
};
#define JsonRpcRequest_Fields (jsonrpc)(method)(params)(id)
QN_FUSION_DECLARE_FUNCTIONS(JsonRpcRequest, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(JsonRpcRequest, JsonRpcRequest_Fields);

struct NX_VMS_API JsonRpcError
{
    // Standard defined error codes for `code` field.
    enum Code
    {
        InvalidJson = -32700,
        InvalidRequest = -32600,
        MethodNotFound = -32601,
        InvalidParams = -32602,
        InternalError = -32603,
        RequestError = 0,
    };

    int code = RequestError;
    std::string message;

    /**%apidoc:any */
    std::optional<QJsonValue> data;
};
#define JsonRpcError_Fields (code)(message)(data)
QN_FUSION_DECLARE_FUNCTIONS(JsonRpcError, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(JsonRpcError, JsonRpcError_Fields);

struct NX_VMS_API JsonRpcResponse
{
    /*const*/ std::string jsonrpc = "2.0";

    /*
     * TODO: Replace with std::variant<int, std::string, QJsonValue(QJsonValue::Null)> when
     * apidoctool will support this.
     */
    /**%apidoc:any */
    QJsonValue id;

    /**%apidoc:any */
    std::optional<QJsonValue> result;
    std::optional<JsonRpcError> error;
};
#define JsonRpcResponse_Fields (jsonrpc)(id)(result)(error)
QN_FUSION_DECLARE_FUNCTIONS(JsonRpcResponse, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(JsonRpcResponse, JsonRpcResponse_Fields);

} // namespace nx::vms::api
