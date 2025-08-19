// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>
#include <variant>

#include <rapidjson/document.h>

#include <nx/reflect/instrument.h>
#include <nx/utils/json/exceptions.h>
#include <nx/utils/json/json.h>

// JSON RPC 2.0 Specification: https://www.jsonrpc.org/specification

namespace nx::json_rpc {

struct NumericId
{
    /**%apidoc[unused] */
    int64_t value;

    NumericId(const NumericId&) = default;
    NumericId(NumericId&&) = default;
    NumericId& operator=(const NumericId&) = default;
    NumericId& operator=(NumericId&&) = default;

    NumericId(int64_t other = 0): value(other) {}
    NumericId& operator=(int64_t other)
    {
        value = other;
        return *this;
    }

    operator int64_t() const { return value; }

    NumericId& operator++()
    {
        ++value;
        return *this;
    }

    NumericId operator++(int)
    {
        NumericId r = *this;
        ++value;
        return r;
    }

    QString toString() const { return QString::number(value); }
};

inline nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& context, NumericId* data)
{
    if (context.value.IsInt64())
    {
        *data = context.value.GetInt64();
        return {};
    }

    if (context.value.IsInt())
    {
        *data = context.value.GetInt();
        return {};
    }

    if constexpr (sizeof(std::decay_t<decltype(context.value.GetUint())>) < 8u)
    {
        if (context.value.IsUint())
        {
            *data = context.value.GetUint();
            return {};
        }
    }

    return {false, "Signed 64 bit integer expected",
        nx::reflect::json_detail::getStringRepresentation(context.value)};
}

template<typename SerializationContext>
inline void serialize(SerializationContext* context, const NumericId& value)
{
    context->composer.writeInt(value.value);
}

using StringId = QString;
using RequestId = std::optional<std::variant<StringId, NumericId>>;
using ResponseId = std::variant<StringId, NumericId, std::nullptr_t>;

template<typename T>
T deserializedOrThrow(rapidjson::Value& value, std::string_view name)
{
    using namespace nx::reflect::json;
    using namespace nx::json;

    T data;
    auto r = nx::reflect::json::deserialize(DeserializationContext{value}, &data);
    if (r)
        return data;

    if (!r.firstNonDeserializedField)
    {
        throw InvalidParameterException{{
            QString::fromStdString(std::string{name}),
            QString::fromStdString(r.errorDescription)}};
    }

    throw InvalidParameterException{{
        QString::fromStdString(*r.firstNonDeserializedField),
        QString::fromStdString(r.firstBadFragment)}};
}

/**%apidoc
 * %param[opt]:{std::variant<QJsonObject, QJsonArray>} params
 */
struct NX_JSON_RPC_API Request
{
    /**%apidoc[unused]
     * Holder of allocator for parsed params and extensions fields. The allocator is refcounted.
     */
    std::optional<rapidjson::Document::AllocatorType> allocator;

    /**%apidoc
     * %value "2.0"
     */
    std::string jsonrpc = "2.0";

    /**%apidoc:{std::optional<std::variant<QString, int>>} */
    RequestId id;

    /**%apidoc
     * %example rest.v{1-}.servers.info.all
     */
    std::string method;

    mutable std::optional<rapidjson::Value> params;

    /**%apidoc[proprietary]:object */
    std::optional<rapidjson::Value> extensions;

    template<typename T>
    static Request create(RequestId id, std::string method, const T& data)
    {
        return create(
            std::move(id), std::move(method), nx::json::serialized(data, /*stripDefault*/ false));
    }

    static Request create(RequestId id, std::string method, rapidjson::Document serialized)
    {
        return {
            .allocator = std::move(serialized.GetAllocator()),
            .id = std::move(id),
            .method = std::move(method),
            .params = std::move(serialized.Move())};
    }

    static Request create(RequestId id, std::string method)
    {
        return {.id = std::move(id), .method = std::move(method)};
    }

    template<typename T>
    T parseOrThrow() const
    {
        if (params)
            return deserializedOrThrow<T>(*params, "params");

        throw nx::json::InvalidParameterException{{"params", "Must be specified"}};
    }

    ResponseId responseId() const
    {
        ResponseId result{std::nullptr_t()};
        if (id)
        {
            if (std::holds_alternative<NumericId>(*id))
                result = std::get<NumericId>(*id);
            else
                result = std::get<StringId>(*id);
        }
        return result;
    }

    Request copy() const
    {
        Request r{.id = id, .method = method};
        // Use new allocator as copy is usually required for another thread.
        if (params || extensions)
            r.allocator = rapidjson::Document::AllocatorType{};
        if (params)
            r.params = rapidjson::Value(*params, *r.allocator, /*copyConstStrings*/ true);
        if (extensions)
            r.extensions = rapidjson::Value(*extensions, *r.allocator, /*copyConstStrings*/ true);
        return r;
    }
};
#define nx_json_rpc_Request_Fields (jsonrpc)(id)(method)(params)(extensions)
NX_REFLECTION_INSTRUMENT(Request, nx_json_rpc_Request_Fields)
inline nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& context, Request* data)
{
    NX_ASSERT(data->allocator);
    return nx::reflect::json_detail::deserialize(context, data);
}

/**%apidoc
 * %param[opt]:any data
 */
struct NX_JSON_RPC_API Error
{
    // Standard defined error codes for `code` field.
    // Standard codes: https://www.jsonrpc.org/specification#error_object
    // XML-RPC: https://xmlrpc-epi.sourceforge.net/specs/rfc.fault_codes.php
    enum Code
    {
        parseError = -32700,
        encodingError = -32701,
        charsetError = -32702,

        invalidRequest = -32600,
        methodNotFound = -32601,
        invalidParams = -32602,
        internalError = -32603,

        applicationError = -32500,
        systemError = -32400,
        transportError = -32300,

        serverErrorBegin = -32099,
        serverErrorEnd = -32000,

        reservedErrorBegin = -32768,
        reservedErrorEnd = -32000,
    };

    int code = systemError;
    std::string message;

    // Must be allocated on Response::allocator.
    mutable std::optional<rapidjson::Value> data;
};
#define nx_json_rpc_Error_Fields (code)(message)(data)
NX_REFLECTION_INSTRUMENT(Error, nx_json_rpc_Error_Fields)

/**%apidoc
 * %param[opt]:any result
 */
struct NX_JSON_RPC_API Response
{
    /**%apidoc[unused]
     * Holder of allocator for parsed result or error.data fields. The allocator is refcounted.
     */
    std::optional<rapidjson::Document::AllocatorType> allocator;

    /**%apidoc
     * %value "2.0"
     */
    std::string jsonrpc = "2.0";

    /**%apidoc:{std::variant<QString, int, std::nullptr_t>} */
    ResponseId id{std::nullptr_t()};

    mutable std::optional<rapidjson::Value> result;

    std::optional<Error> error;

    /**%apidoc[proprietary]:object */
    std::optional<rapidjson::Value> extensions;

    template<typename T>
    static Response makeError(ResponseId id, int code, std::string message, const T& data)
    {
        return makeError(std::move(id), code, std::move(message),
            nx::json::serialized(data, /*stripDefault*/ false));
    }

    static Response makeError(ResponseId id, int code, std::string message, rapidjson::Document serialized)
    {
        return {
            .allocator = std::move(serialized.GetAllocator()),
            .id = std::move(id),
            .error = Error{code, std::move(message), std::move(serialized.Move())}};
    }

    static Response makeError(ResponseId id, int code, std::string message)
    {
        return {.id = std::move(id), .error = Error{code, std::move(message)}};
    }

    template<typename T>
    static Response makeResult(ResponseId id, T&& data)
    {
        auto serialized{nx::json::serialized(data, /*stripDefault*/ false)};
        return {
            .allocator = std::move(serialized.GetAllocator()),
            .id = std::move(id),
            .result = std::move(serialized.Move())};
    }

    template<typename T>
    T resultOrThrow() const
    {
        if (result)
            return deserializedOrThrow<T>(*result, "result");

        throw nx::json::InvalidParameterException{{"result", "Must be provided"}};
    }

    Response copy() const
    {
        Response r{.id = id};
        // Use new allocator as copy is usually required for another thread.
        if (result || (error && error->data) || extensions)
            r.allocator = rapidjson::Document::AllocatorType{};
        if (error)
        {
            r.error = Error{.code = error->code, .message = error->message};
            if (error->data)
            {
                r.error->data =
                    rapidjson::Value(*error->data, *r.allocator, /*copyConstStrings*/ true);
            }
        }
        else if (result)
        {
            r.result = rapidjson::Value(*result, *r.allocator, /*copyConstStrings*/ true);
        }
        if (extensions)
            r.extensions = rapidjson::Value(*extensions, *r.allocator, /*copyConstStrings*/ true);
        return r;
    }
};
#define nx_json_rpc_Response_Fields (jsonrpc)(id)(result)(error)(extensions)
NX_REFLECTION_INSTRUMENT(Response, nx_json_rpc_Response_Fields)
inline nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& context, Response* data)
{
    NX_ASSERT(data->allocator);
    auto r = nx::reflect::json_detail::deserialize(context, data);
    if (data->error && data->result)
        data->result.reset(); //< Ignore result in case of error.
    return r;
}

} // namespace nx::json_rpc

namespace std {

template<>
struct hash<nx::json_rpc::NumericId>
{
    size_t operator()(const nx::json_rpc::NumericId& id) const
    {
        return hash<int64_t>()(id.value);
    }
};

} // namespace std
