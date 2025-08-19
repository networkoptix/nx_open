// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/json_rpc/messages.h>
#include <nx/network/rest/response.h>

namespace nx::network::rest::json_rpc {

struct ClientExtensions
{
    std::optional<std::string> etag;
};
NX_REFLECTION_INSTRUMENT(ClientExtensions, (etag))

template<typename T>
struct Payload
{
    std::optional<T> data;
    std::optional<ClientExtensions> extensions;
};
NX_REFLECTION_INSTRUMENT_TEMPLATE(Payload, (data)(extensions))

template<typename T>
inline nx::json_rpc::Request to(nx::json_rpc::RequestId id, std::string method, Payload<T> payload)
{
    auto result = nx::json_rpc::Request::create(std::move(id), std::move(method));
    if (payload.data || payload.extensions)
    {
        auto document = nx::json::serialized(payload, /*stripDefault*/ false);
        result.allocator = std::move(document.GetAllocator());
        if (payload.data)
            result.params = std::move(document["data"].Move());
        if (payload.extensions)
            result.extensions = std::move(document["extensions"].Move());
    }
    return result;
}

template<typename T>
inline Payload<T> from(nx::json_rpc::Request origin)
{
    Payload<T> result;
    if (origin.params)
    {
        T data;
        if (nx::reflect::json::deserialize({*origin.params}, &data))
            result.data = std::move(data);
    }
    if (origin.extensions)
    {
        ClientExtensions data;
        if (nx::reflect::json::deserialize({*origin.extensions}, &data))
            result.extensions = std::move(data);
    }
    return result;
}

NX_NETWORK_REST_API nx::json_rpc::Response to(
    nx::json_rpc::ResponseId id, nx::network::rest::Response response);

struct Error
{
    int code = nx::json_rpc::Error::Code::systemError;
    std::string message;
    std::optional<nx::network::rest::Result> details;
};

template<typename T>
using Response = std::variant<Error, Payload<T>>;

template<typename T>
inline Response<T> from(nx::json_rpc::Response origin)
{
    if (origin.error)
    {
        Error result{origin.error->code, std::move(origin.error->message)};
        if (origin.error->data)
        {
            nx::network::rest::Result details;
            if (nx::reflect::json::deserialize({*origin.error->data}, &details))
                result.details = std::move(details);
        }
        return result;
    }

    Payload<T> result;
    if (origin.result)
    {
        T data;
        if (nx::reflect::json::deserialize({*origin.result}, &data))
            result.data = std::move(data);
    }
    if (origin.extensions)
    {
        ClientExtensions data;
        if (nx::reflect::json::deserialize({*origin.extensions}, &data))
            result.extensions = std::move(data);
    }
    return result;
}

} // namespace nx::network::rest::json_rpc
