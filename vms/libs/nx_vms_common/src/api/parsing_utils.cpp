// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "parsing_utils.h"

namespace rest {

bool isSessionExpiredError(nx::network::rest::ErrorId code)
{
    return code == nx::network::rest::ErrorId::sessionExpired
        || code == nx::network::rest::ErrorId::sessionRequired;
}

bool isSessionExpiredError(const nx::json_rpc::Response& response)
{
    if (!response.error)
        return false;
    if (!response.error->data)
        return false;

    nx::network::rest::Result result;
    if (!nx::reflect::json::deserialize(
        nx::reflect::json::DeserializationContext{*response.error->data}, &result))
    {
        return false;
    }

    return isSessionExpiredError(result.errorId);
}

nx::reflect::DeserializationResult deserializeResponse(
    const std::string_view& json,
    rest::JsonRpcResultType* data,
    [[maybe_unused]] nx::reflect::json::DeserializationFlag skipErrors)
{
    rapidjson::Document document;
    document.Parse(json.data(), json.size());
    if (document.HasParseError())
    {
        return nx::reflect::DeserializationResult(
            false, nx::reflect::json_detail::parseErrorToString(document), std::string(json));
    }

    if (document.IsObject())
    {
        nx::json_rpc::Response response{document.GetAllocator()};
        auto r{nx::reflect::json::deserialize({document.Move()}, &response)};
        if (r)
            *data = std::move(response);
        return r;
    }

    if (!document.IsArray())
        return nx::reflect::DeserializationResult(false, "Must be array or object", std::string{json});

    std::vector<nx::json_rpc::Response> responses;
    responses.reserve(document.Size());
    for (int i = 0; i < (int) document.Size(); ++i)
    {
        nx::json_rpc::Response response{document.GetAllocator()};
        auto r{nx::reflect::json::deserialize({document[i]}, &response)};
        if (!r)
        {
            return nx::reflect::DeserializationResult(false,
                NX_FMT("Failed to deserialize response item %1: %2", i, r.errorDescription)
                    .toStdString(),
                r.firstBadFragment,
                r.firstNonDeserializedField);
        }
        responses.push_back(std::move(response));
    }
    *data = std::move(responses);
    return {};
}

} // namespace rest
