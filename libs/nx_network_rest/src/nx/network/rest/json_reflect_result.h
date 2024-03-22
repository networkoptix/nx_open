// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/json.h>

#include "result.h"

namespace nx::network::rest {

template<typename T>
struct JsonReflectResult: Result
{
    T reply;
};
#define JsonReflectResult_Fields Result_Fields (reply)

inline void serialize(nx::reflect::json::SerializationContext* ctx, const Result::Error& data)
{
    nx::reflect::json::serialize(ctx, std::to_string((int) data));
}

inline nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& ctx, Result::Error* data)
{
    int tmp = Result::Error::NoError;
    const auto result = nx::reflect::json::deserialize(ctx, &tmp);
    if (!result)
        return result;

    if (tmp >= Result::Error::NoError && tmp <= Result::Error::Unauthorized)
    {
        *data = (Result::Error) tmp;
        return result;
    }

    return nx::reflect::DeserializationResult(/*result*/ false,
        /*errorDescription*/ "Out of range",
        nx::reflect::json_detail::getStringRepresentation(ctx.value),
        /*notDeserializedField*/ "error");
}

NX_REFLECTION_INSTRUMENT_TEMPLATE(JsonReflectResult, JsonReflectResult_Fields)

} // namespace nx::network::rest
