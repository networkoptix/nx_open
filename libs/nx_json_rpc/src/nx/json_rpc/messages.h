// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>
#include <variant>

#include <rapidjson/document.h>

#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/json.h>
#include <nx/utils/serialization/json_exceptions.h>

// JSON RPC 2.0 Specification: https://www.jsonrpc.org/specification

namespace nx::json_rpc {

using ResponseId = std::variant<QString, int, std::nullptr_t>;

template<typename T>
T deserializedOrThrow(rapidjson::Value& value, std::string_view name)
{
    using namespace nx::reflect::json;
    using namespace nx::utils::serialization::json;

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

struct NX_JSON_RPC_API Request
{
    /**%apidoc[unused] Parsed document holder for `params`. */
    std::shared_ptr<rapidjson::Document> document;

    /**%apidoc
     * %value "2.0"
     */
    std::string jsonrpc = "2.0";

    std::optional<std::variant<QString, int>> id;

    /**%apidoc
     * %example rest.v{1-}.servers.info.list
     */
    std::string method;

    /**%apidoc[opt]:{std::variant<QJsonObject, QJsonArray>} */
    rapidjson::Value* params = nullptr;

    template<typename T>
    static Request create(
        std::optional<std::variant<QString, int>> id, std::string method, const T& data)
    {
        auto serialized{nx::utils::serialization::json::serialized(data, /*stripDefault*/ false)};
        auto allocator{serialized.GetAllocator()};
        Request r{
            .document = std::make_shared<rapidjson::Document>(&allocator),
            .id = std::move(id),
            .method = std::move(method)};
        rapidjson::Value value;
        value.Swap(serialized);
        serialized.SetObject();
        r.document->Swap(serialized);
        r.document->AddMember("jsonrpc", r.jsonrpc, allocator);
        if (r.id)
        {
            r.document->AddMember("id",
                std::holds_alternative<int>(*r.id)
                    ? rapidjson::Value{std::get<int>(*r.id)}
                    : rapidjson::Value{std::get<QString>(*r.id).toStdString(), allocator},
                allocator);
        }
        r.document->AddMember("method", r.method, allocator);
        r.document->AddMember("params", std::move(value), allocator);
        r.params = &(*r.document)["params"];
        return r;
    }

    static Request create(std::optional<std::variant<QString, int>> id, std::string method)
    {
        Request r{
            .document = std::make_shared<rapidjson::Document>(rapidjson::kObjectType),
            .id = std::move(id),
            .method = std::move(method)};
        auto allocator{r.document->GetAllocator()};
        r.document->AddMember("jsonrpc", r.jsonrpc, allocator);
        if (r.id)
        {
            r.document->AddMember("id",
                std::holds_alternative<int>(*r.id)
                    ? rapidjson::Value{std::get<int>(*r.id)}
                    : rapidjson::Value{std::get<QString>(*r.id).toStdString(), allocator},
                allocator);
        }
        r.document->AddMember("method", r.method, allocator);
        return r;
    }

    template<typename T>
    T parseOrThrow() const
    {
        if (params)
            return deserializedOrThrow<T>(*params, "params");

        throw nx::utils::serialization::json::InvalidParameterException{
            {"params", "Must be specified"}};
    }

    ResponseId responseId() const
    {
        ResponseId result{std::nullptr_t()};
        if (id)
        {
            if (std::holds_alternative<int>(*id))
                result = std::get<int>(*id);
            else
                result = std::get<QString>(*id);
        }
        return result;
    }

    nx::reflect::DeserializationResult deserialize(rapidjson::Value& json)
    {
        NX_ASSERT(document, "Holder must be initialized");
        if (!json.IsObject())
        {
            return {false,
                "Object value expected", nx::reflect::json_detail::getStringRepresentation(json)};
        }

        auto r = nx::reflect::json_detail::Deserializer{
                nx::reflect::json::DeserializationContext{json}, this
            }(
                nx::reflect::WrappedMemberVariableField("id", &Request::id),
                nx::reflect::WrappedMemberVariableField("method", &Request::method)
            );
        if (r)
        {
            auto it = json.FindMember("params");
            if (it != json.MemberEnd())
                params = &it->value;
        }
        return r;
    }
};

template<typename SerializationContext>
inline void serialize(SerializationContext* context, const Request& value)
{
    nx::reflect::Visitor<SerializationContext, Request>{context, value}(
        nx::reflect::WrappedMemberVariableField("jsonrpc", &Request::jsonrpc),
        nx::reflect::WrappedMemberVariableField("id", &Request::id),
        nx::reflect::WrappedMemberVariableField("method", &Request::method),
        nx::reflect::WrappedMemberVariableField("params", &Request::params));
}

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

    /**%apidoc[opt]:{QJsonValue} Must be allocated on Response::document. */
    rapidjson::Value* data = nullptr;

    nx::reflect::DeserializationResult deserialize(rapidjson::Value& json)
    {
        if (!json.IsObject())
        {
            return {false,
                "Object value expected",
                nx::reflect::json_detail::getStringRepresentation(json)};
        }

        auto r = nx::reflect::json_detail::Deserializer{
                nx::reflect::json::DeserializationContext{json}, this
            }(
                nx::reflect::WrappedMemberVariableField("code", &Error::code),
                nx::reflect::WrappedMemberVariableField("message", &Error::message)
            );
        if (r)
        {
            auto it = json.FindMember("data");
            if (it != json.MemberEnd())
                data = &it->value;
        }
        return r;
    }
};

template<typename SerializationContext>
inline void serialize(SerializationContext* context, const Error& value)
{
    nx::reflect::Visitor<SerializationContext, Error>{context, value}(
        nx::reflect::WrappedMemberVariableField("code", &Error::code),
        nx::reflect::WrappedMemberVariableField("message", &Error::message),
        nx::reflect::WrappedMemberVariableField("data", &Error::data));
}

struct NX_JSON_RPC_API Response
{
    /**%apidoc[unused] Parsed document holder for `result`. */
    std::shared_ptr<rapidjson::Document> document;

    /**%apidoc
     * %value "2.0"
     */
    std::string jsonrpc = "2.0";

    // TODO: Use `ResponseId` when apidoctool will support `using`.
    std::variant<QString, int, std::nullptr_t> id{std::nullptr_t()};

    /**%apidoc[opt]:{QJsonValue} */
    rapidjson::Value* result = nullptr;

    std::optional<Error> error;

    template<typename T>
    static Response makeError(ResponseId id, int code, std::string message, const T& data)
    {
        auto serialized{nx::utils::serialization::json::serialized(data, /*stripDefault*/ false)};
        auto allocator{serialized.GetAllocator()};
        Response r{
            .document = std::make_shared<rapidjson::Document>(&allocator), .id = std::move(id)};
        rapidjson::Value value;
        value.Swap(serialized);
        serialized.SetObject();
        r.document->Swap(serialized);
        r.document->AddMember("jsonrpc", r.jsonrpc, allocator);
        r.document->AddMember("id",
            std::holds_alternative<std::nullptr_t>(r.id)
                ? rapidjson::Value{}
                : std::holds_alternative<int>(r.id)
                    ? rapidjson::Value{std::get<int>(r.id)}
                    : rapidjson::Value{std::get<QString>(r.id).toStdString(), allocator},
            allocator);
        rapidjson::Value errorValue{rapidjson::Type::kObjectType};
        errorValue.AddMember("code", code, allocator);
        errorValue.AddMember("message", message, allocator);
        errorValue.AddMember("data", std::move(value), allocator);
        r.document->AddMember("error", std::move(errorValue), allocator);
        r.error = Error{code, std::move(message)};
        r.error->data = &(*r.document)["error"]["data"];
        return r;
    }

    static Response makeError(ResponseId id, int code, std::string message)
    {
        Response r{
            .document = std::make_shared<rapidjson::Document>(rapidjson::kObjectType),
            .id = std::move(id)};
        auto allocator{r.document->GetAllocator()};
        r.document->AddMember("jsonrpc", r.jsonrpc, allocator);
        r.document->AddMember("id",
            std::holds_alternative<std::nullptr_t>(r.id) ? rapidjson::Value{}
                : std::holds_alternative<int>(r.id)
                    ? rapidjson::Value{std::get<int>(r.id)}
                    : rapidjson::Value{std::get<QString>(r.id).toStdString(), allocator},
            allocator);
        rapidjson::Value errorValue{rapidjson::Type::kObjectType};
        errorValue.AddMember("code", code, allocator);
        errorValue.AddMember("message", message, allocator);
        r.document->AddMember("error", std::move(errorValue), allocator);
        r.error = Error{code, std::move(message)};
        return r;
    }

    template<typename T>
    static Response makeResult(ResponseId id, T&& data)
    {
        auto serialized{nx::utils::serialization::json::serialized(data, /*stripDefault*/ false)};
        auto allocator{serialized.GetAllocator()};
        Response r{
            .document = std::make_shared<rapidjson::Document>(&allocator), .id = std::move(id)};
        rapidjson::Value value;
        value.Swap(serialized);
        serialized.SetObject();
        r.document->Swap(serialized);
        r.document->AddMember("jsonrpc", r.jsonrpc, allocator);
        r.document->AddMember("id",
            std::holds_alternative<std::nullptr_t>(r.id) ? rapidjson::Value{}
                : std::holds_alternative<int>(r.id)
                    ? rapidjson::Value{std::get<int>(r.id)}
                    : rapidjson::Value{std::get<QString>(r.id).toStdString(), allocator},
            allocator);
        r.document->AddMember("result", std::move(value), allocator);
        r.result = &(*r.document)["result"];
        return r;
    }

    template<typename T>
    T resultOrThrow() const
    {
        if (result)
            return deserializedOrThrow<T>(*result, "result");

        throw nx::utils::serialization::json::InvalidParameterException{
            {"result", "Must be provided"}};
    }

    nx::reflect::DeserializationResult deserialize(rapidjson::Value& json)
    {
        NX_ASSERT(document, "Holder must be initialized");
        if (!json.IsObject())
        {
            return {false,
                "Object value expected",
                nx::reflect::json_detail::getStringRepresentation(json)};
        }

        auto r = nx::reflect::json_detail::Deserializer{
                nx::reflect::json::DeserializationContext{json}, this
            }(
                nx::reflect::WrappedMemberVariableField("id", &Response::id)
            );
        if (!r)
            return r;

        if (auto it = json.FindMember("result"); it != json.MemberEnd())
            result = &it->value;
        if (auto it = json.FindMember("error"); it != json.MemberEnd())
        {
            Error e;
            r = e.deserialize(it->value);
            if (!r)
            {
                if (r.firstNonDeserializedField)
                    r.firstNonDeserializedField = "error." + *r.firstNonDeserializedField;
                else
                    r.firstNonDeserializedField = "error";
                return r;
            }

            error = std::move(e);
        }
        return {};
    }
};

template<typename SerializationContext>
inline void serialize(SerializationContext* context, const Response& value)
{
    nx::reflect::Visitor<SerializationContext, Response>{context, value}(
        nx::reflect::WrappedMemberVariableField("jsonrpc", &Response::jsonrpc),
        nx::reflect::WrappedMemberVariableField("id", &Response::id),
        nx::reflect::WrappedMemberVariableField("result", &Response::result),
        nx::reflect::WrappedMemberVariableField("error", &Response::error));
}

} // namespace nx::json_rpc
