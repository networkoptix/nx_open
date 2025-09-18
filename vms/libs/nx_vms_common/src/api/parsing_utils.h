// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <expected>
#include <string_view>

#include <api/http_client_pool.h>
#include <api/server_rest_connection_fwd.h>
#include <nx/vms/common/api/helpers/parser_helper.h>

#include "rest_types.h"

namespace rest {

static constexpr int kMessageBodyLogSize = 50;

bool isSessionExpiredError(nx::network::rest::ErrorId code);
bool isSessionExpiredError(const nx::json_rpc::Response& response);

// Type helper for parsing function overloading.
template <typename T>
struct Type {};

// Response deserialization for ResultWithData objects.
template<typename T>
rest::ResultWithData<T> parseMessageBody(
    Type<rest::ResultWithData<T>>,
    Qn::SerializationFormat format,
    const QByteArray& msgBody,
    const nx::network::http::StatusLine& statusLine,
    bool* success)
{
    using Result = rest::ResultWithData<T>;

    if constexpr (!std::is_same_v<T, rest::EmptyResponseType>)
    {
        switch (format)
        {
            case Qn::SerializationFormat::json:
            {
                auto restResult =
                    QJson::deserialized(msgBody, nx::network::rest::JsonResult(), success);
                return Result(restResult, restResult.deserialized<T>());
            }
            case Qn::SerializationFormat::ubjson:
            {
                auto restResult =
                    QnUbjson::deserialized(msgBody, nx::network::rest::UbjsonResult(), success);
                return Result(restResult, restResult.deserialized<T>());
            }
            default:
                break;
        }
    }
    else
    {
        *success = nx::network::http::StatusCode::isSuccessCode(statusLine.statusCode);
        return {};
    }

    *success = false;

    NX_DEBUG(NX_SCOPE_TAG,
        "Unsupported format '%1', status code: %2, message body: %3 ...",
        nx::reflect::enumeration::toString(format),
        statusLine,
        msgBody.left(kMessageBodyLogSize));

    return Result();
}

// Response deserialization for plain object types.
template<typename T>
T parseMessageBody(
    Type<T>,
    Qn::SerializationFormat format,
    const QByteArray& msgBody,
    const nx::network::http::StatusLine& statusLine,
    bool* success)
{
    if (statusLine.statusCode != nx::network::http::StatusCode::ok)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Unexpected HTTP status code: %1", statusLine);
        *success = false;
        return T();
    }

    if constexpr (!std::is_same_v<T, rest::EmptyResponseType>)
    {
        switch (format)
        {
            case Qn::SerializationFormat::json:
                return QJson::deserialized(msgBody, T(), success);
            case Qn::SerializationFormat::ubjson:
                return QnUbjson::deserialized(msgBody, T(), success);
            default:
                break;
        }
    }
    else
    {
        *success = nx::network::http::StatusCode::isSuccessCode(statusLine.statusCode);
        return {};
    }

    *success = false;

    NX_DEBUG(NX_SCOPE_TAG,
        "Unsupported format '%1', status code: %2, message body: %3 ...",
        nx::reflect::enumeration::toString(format),
        statusLine,
        msgBody.left(kMessageBodyLogSize));

    return T();
}

using JsonRpcResultType =
    std::variant<nx::json_rpc::Response, std::vector<nx::json_rpc::Response>>;

nx::reflect::DeserializationResult deserializeResponse(
    std::string_view json,
    rest::JsonRpcResultType* data,
    nx::reflect::json::DeserializationFlag skipErrors = nx::reflect::json::DeserializationFlag::none);

// Response deserialization for ErrorOrData objects
template<typename T>
rest::ErrorOrData<T> parseMessageBody(
    Type<rest::ErrorOrData<T>>,
    Qn::SerializationFormat format,
    const QByteArray& messageBody,
    const nx::network::http::StatusLine& statusLine,
    bool* success)
{
    // Unsupported format is OK for raw requests, no need to write it to the log.
    if constexpr (!std::is_same_v<T, QByteArray>)
    {
        if (format != Qn::SerializationFormat::json)
        {
            NX_DEBUG(NX_SCOPE_TAG,
                "Unsupported format '%1', status code: %2, message body: %3 ...",
                nx::reflect::enumeration::toString(format),
                statusLine,
                messageBody.left(kMessageBodyLogSize));
        }
    }

    if (statusLine.statusCode == nx::network::http::StatusCode::ok)
    {
        if constexpr (std::is_same_v<T, rest::EmptyResponseType>)
        {
            *success = true;
            return T();
        }
        else if constexpr (std::is_same_v<T, QByteArray>)
        {
            *success = true;
            return messageBody;
        }
        else if constexpr (std::is_same_v<T, JsonRpcResultType>)
        {
            T data;
            *success = deserializeResponse(
                std::string_view(messageBody.data(), messageBody.size()),
                &data);

            if (*success)
                return data;

            NX_ASSERT(false, "Data cannot be deserialized:\n %1\nType: %2",
                messageBody.left(kMessageBodyLogSize), typeid(T).name());
            return std::unexpected(nx::network::rest::Result::notImplemented());
        }
        else
        {
            using V = std::variant<T, nx::network::rest::Result>;
            auto [data, result] = nx::reflect::json::deserialize<V>(messageBody.data());
            *success = result;
            if (result)
            {
                if (std::holds_alternative<T>(data))
                    return std::get<T>(data);
                else
                    return std::unexpected(std::get<nx::network::rest::Result>(data));
            }

            NX_ASSERT(false, "Data cannot be deserialized:\n %1\nType: %2",
                messageBody.left(kMessageBodyLogSize), typeid(T).name());
            return std::unexpected(nx::network::rest::Result::notImplemented());
        }
    }

    auto result = nx::vms::common::api::parseRestResult(statusLine.statusCode, format, messageBody);
    *success = (result.errorId == nx::network::rest::ErrorId::ok);

    return std::unexpected(result);
}

template <typename T>
T parseMessageBody(
    Qn::SerializationFormat format,
    const QByteArray& messageBody,
    const nx::network::http::StatusLine& statusLine,
    bool* success)
{
    NX_CRITICAL(success);
    return parseMessageBody(Type<T>{}, format, messageBody, statusLine, success);
}

template<typename T>
T parseJsonBody(
    Type<T>,
    rapidjson::Value*,
    bool* success)
{
    *success = false;
    return {};
}

template<typename T>
rest::ErrorOrData<T> parseJsonBody(
    Type<rest::ErrorOrData<T>>,
    rapidjson::Value* value,
    bool* success)
{
    if constexpr (!std::is_same_v<T, rest::EmptyResponseType>)
    {
        std::variant<T, nx::network::rest::Result> data;

        auto r = nx::reflect::json::deserialize(
            nx::reflect::json_detail::DeserializationContext{*value},
            &data);

        *success = r;

        if (r)
        {
            if (std::holds_alternative<T>(data))
                return std::get<T>(data);
            else
                return std::unexpected(std::get<nx::network::rest::Result>(data));
        }

        NX_ASSERT(false, "Data cannot be deserialized:\n %1\nType: %2",
            value, typeid(T).name());

        return std::unexpected(nx::network::rest::Result::notImplemented());
    }
    else
    {
        *success = true;
        return T();
    }

}

template <typename T>
T parseJsonValue(
    rapidjson::Value* value,
    bool* success)
{
    NX_CRITICAL(success);
    return parseJsonBody(Type<T>{}, value, success);
}

class BaseResultContext
{
public:
    BaseResultContext() {}
    virtual ~BaseResultContext() {}

    virtual void parse(
        [[maybe_unused]] Qn::SerializationFormat format,
        [[maybe_unused]] const QByteArray& messageBody,
        [[maybe_unused]] SystemError::ErrorCode systemError,
        [[maybe_unused]] const nx::network::http::StatusLine& statusLine,
        [[maybe_unused]] const nx::network::http::HttpHeaders& headers)
    {
    }

    virtual void parse([[maybe_unused]] const nx::json_rpc::Response& response)
    {
    }

    virtual nx::network::http::ClientPool::Request fixup(
        const nx::network::http::ClientPool::Request& request)
    {
        return request;
    }

    virtual bool isSessionExpired() const = 0;

    virtual bool isSuccess() const = 0;

    void setToken(nx::network::http::AuthToken token)
    {
        m_token = token;
    }

    std::optional<nx::network::http::AuthToken> token() const
    {
        return m_token;
    }

private:
    std::optional<nx::network::http::AuthToken> m_token; //< Token which was used for this request.
};

using RequestCallback =
    nx::MoveOnlyFunc<void (std::unique_ptr<BaseResultContext>, rest::Handle)>;

template <typename T>
class ResultContext: public BaseResultContext
{
public:
    void parse(
        Qn::SerializationFormat format,
        const QByteArray& messageBody,
        SystemError::ErrorCode systemError,
        const nx::network::http::StatusLine& statusLine,
        const nx::network::http::HttpHeaders&) override
    {
        // Server may not specify format for empty responses.
        if (messageBody.isEmpty() && format == Qn::SerializationFormat::unsupported)
            format = Qn::SerializationFormat::json;

        result = parseMessageBody<T>(format, messageBody, statusLine, &success);

        if (systemError != SystemError::noError
            || statusLine.statusCode != nx::network::http::StatusCode::ok)
        {
            success = false;
        }

        // Check format for the non-raw requests.
        if constexpr (!std::is_same_v<T, rest::ErrorOrData<QByteArray>>)
        {
            const bool goodFormat = format == Qn::SerializationFormat::json
                || format == Qn::SerializationFormat::ubjson;
            if (!goodFormat)
                success = false;
        }
    }

    template<class>
    struct canHoldRestResult: std::false_type {};

    template<class... Ts>
    struct canHoldRestResult<std::expected<Ts...>>:
        std::bool_constant<(std::is_same_v<nx::network::rest::Result, Ts> || ...)> {};

    static_assert(canHoldRestResult<rest::ErrorOrData<rest::EmptyResponseType>>::value,
        "std::expected must be handled by canHoldRestResult");

    void parse([[maybe_unused]] const nx::json_rpc::Response& response) override
    {
        if (!response.error && response.result)
        {
            result = parseJsonValue<T>(&response.result.value(), &success);
            return;
        }

        success = false;
        if constexpr (canHoldRestResult<T>::value)
        {
            if (isSessionExpiredError(response))
            {
                result = std::unexpected(
                    nx::network::rest::Result(nx::network::rest::ErrorId::sessionRequired));
                return;
            }

            // TODO: translate JSON-RPC error to rest::Result using code in UserGroupRequestChain.
            if (response.error->code == nx::json_rpc::Error::applicationError
                && response.error->data)
            {
                nx::network::rest::Result restError;
                if (nx::reflect::json::deserialize(
                    nx::reflect::json::DeserializationContext{*response.error->data}, &restError))
                {
                    result = std::unexpected(restError);
                    return;
                }
            }

            result = std::unexpected(
                nx::network::rest::Result::serviceUnavailable(
                    QString::fromStdString(response.error->message)));
            return;
        }
        NX_ASSERT(false, "This type is not supported by JSON-RPC");
    }

    bool isSessionExpired() const override
    {
        if constexpr (canHoldRestResult<T>::value)
        {
            if (result && !(*result))
                return isSessionExpiredError((*result).error().errorId);
        }
        else if constexpr (std::is_same_v<nx::network::rest::Result, T>)
        {
            return isSessionExpiredError(result->errorId);
        }

        return false;
    }

    static auto wrapCallback(rest::Callback<T> callback)
    {
        return [callback = std::move(callback)](
                std::unique_ptr<BaseResultContext> resultContext, rest::Handle handle)
            {
                if (!callback)
                    return;
                const auto context = static_cast<ResultContext<T>*>(resultContext.get());

                if (context->result)
                {
                    callback(context->success, handle, std::move(*(context->result)));
                    return;
                }

                // No data parsed.
                if constexpr (canHoldRestResult<T>::value)
                {
                    callback(
                        context->success,
                        handle,
                        std::unexpected(
                            nx::network::rest::Result::cantProcessRequest("Unknown error")));
                }
                else
                {
                    callback(context->success, handle, T());
                }
            };
    }

    virtual bool isSuccess() const override
    {
        return success;
    }

    std::optional<T>&& getResult() &&
    {
        return std::move(result);
    }

private:
    std::optional<T> result;
    bool success = false;
};

class RawResultContext: public BaseResultContext
{
public:
    void parse(
        Qn::SerializationFormat,
        const QByteArray& messageBody,
        SystemError::ErrorCode systemError,
        const nx::network::http::StatusLine& statusLine,
        const nx::network::http::HttpHeaders& resultHeaders) override
    {
        // Need to deep copy buffer, because it is made from raw data.
        // FIXME: Think about moving the buffer straight to the end callback.
        result = QByteArray(messageBody.constData(), messageBody.size());
        headers = resultHeaders;

        if (systemError != SystemError::noError
            || statusLine.statusCode < nx::network::http::StatusCode::ok
            || statusLine.statusCode > nx::network::http::StatusCode::partialContent)
        {
            success = false;
        }
        else
        {
            success = true;
        }
    }

    static auto wrapCallback(DataCallback callback)
    {
        return [callback = std::move(callback)](
                std::unique_ptr<BaseResultContext> resultContext, rest::Handle handle)
            {
                if (!callback)
                    return;
                const auto context = static_cast<RawResultContext*>(resultContext.get());
                callback(context->success, handle, context->result, context->headers);
            };
    }

    virtual bool isSessionExpired() const override
    {
        // RawResultContext is not intended to be used with requests, which potentially could
        // require a fresh session.
        return false;
    }

    virtual bool isSuccess() const override
    {
        return success;
    }

    QByteArray&& getResult() &&
    {
        return std::move(result);
    }

private:
    QByteArray result;
    nx::network::http::HttpHeaders headers;
    bool success = false;
};

} // namespace rest
