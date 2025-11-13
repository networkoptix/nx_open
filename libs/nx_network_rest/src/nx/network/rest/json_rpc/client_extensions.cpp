// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_extensions.h"

#include <nx/utils/json/qjson.h>

namespace nx::network::rest::json_rpc {

namespace {

std::optional<rapidjson::Document> contentBody(nx::network::rest::Response* response)
{
    if (std::holds_alternative<rapidjson::Document>(response->contentBodyJson))
        return std::get<rapidjson::Document>(std::move(response->contentBodyJson));

    if (std::holds_alternative<QJsonValue>(response->contentBodyJson))
    {
        return nx::json::serialized(std::get<QJsonValue>(std::move(response->contentBodyJson)),
            /*stripDefault*/ false);
    }

    auto buffer = response->content ? response->content->body : QByteArray();
    if (buffer.isEmpty())
        return {};

    QJsonValue value;
    QString errorMessage;
    if (!QJsonDetail::deserialize_json(buffer, &value, &errorMessage))
    {
        NX_DEBUG(NX_SCOPE_TAG, "Response is not JSON: " + errorMessage);
        value = QString(buffer);
    }
    return nx::json::serialized(value, /*stripDefault*/ false);
}

std::optional<ClientExtensions> extensions(nx::network::http::HttpHeaders* headers)
{
    std::optional<ClientExtensions> result;
    if (auto it = headers->find("ETag"); it != headers->end() && !it->second.empty())
    {
        if (!result)
            result = ClientExtensions{};
        result->etag = std::move(it->second);
    }
    return result;
}

} // namespace

nx::json_rpc::Response to(nx::json_rpc::ResponseId id, nx::network::rest::Response response)
{
    nx::json_rpc::Response result{.id = std::move(id)};
    auto body = contentBody(&response);
    if (body)
        result.allocator = std::move(body->GetAllocator());
    if (nx::network::http::StatusCode::isSuccessCode(response.statusCode))
    {
        if (body)
            result.result = std::move(body->Move());
        else
            result.result = rapidjson::Value{};
        if (auto e = extensions(&response.httpHeaders))
        {
            auto document = nx::json::serialized(
                *e, /*stripDefault*/ false, body ? &*result.allocator : nullptr);
            if (!result.allocator)
                result.allocator = std::move(document.GetAllocator());
            result.extensions = std::move(document.Move());
        }
    }
    else
    {
        nx::json_rpc::Error error{
            response.statusCode == nx::network::http::StatusCode::unprocessableEntity
                ? nx::json_rpc::Error::invalidParams
                : nx::json_rpc::Error::applicationError,
            "Failed to execute API request"};
        if (body)
            error.data = std::move(body->Move());
        result.error = std::move(error);
    }
    return result;
}

} // namespace nx::network::rest::json_rpc
