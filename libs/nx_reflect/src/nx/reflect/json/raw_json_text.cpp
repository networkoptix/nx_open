// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "raw_json_text.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace nx::reflect::json {

DeserializationResult deserialize(const DeserializationContext& ctx, RawJsonText* data)
{
    // We have to serialize ctx.value back to JSON to get the raw string since the DOM parser is
    // used currently.
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    ctx.value.Accept(writer);
    data->text = buffer.GetString();
    return DeserializationResult(true);
}

void serialize(SerializationContext* ctx, const RawJsonText& data)
{
    ctx->composer.writeRawString(data.text);
}

} // namespace nx::reflect::json
