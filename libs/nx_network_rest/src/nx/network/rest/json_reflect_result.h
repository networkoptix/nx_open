// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/json.h>

#include "result.h"

namespace nx::network::rest {

// Used for fast serialization in some legacy handlers to avoid QJsonValue.
template<typename T>
struct JsonReflectResult: Result
{
    T reply;
};
#define JsonReflectResult_Fields Result_Fields (reply)

template<typename SerializationContext, typename T>
inline void serialize(SerializationContext* context, const JsonReflectResult<T>& value)
{
    context->composer.startObject();
    context->composer.writeAttributeName("errorId");
    context->composer.writeString(nx::reflect::enumeration::toString(value.errorId));
    context->composer.writeAttributeName("errorString");
    context->composer.writeString(value.errorString.toStdString());
    context->composer.writeAttributeName("error");
    context->composer.writeString(std::to_string((int) value.errorId));
    context->composer.writeAttributeName("reply");
    nx::reflect::BasicSerializer::serializeAdl(context, value.reply);
    context->composer.endObject(/*members*/ 4);
}

NX_REFLECTION_INSTRUMENT_TEMPLATE(JsonReflectResult, JsonReflectResult_Fields)

} // namespace nx::network::rest
