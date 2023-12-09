// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include "deserializer.h"
#include "serializer.h"

namespace nx::reflect::json {

/**
 * Reprensent a raw json text. E.g., for the following json:
 *   {"foo":["bar"]}
 * and type
 *   struct Foo { RawJsonText foo; }
 *
 * the RawJsonText variable will contain the text: ["bar"].
 */
struct RawJsonText
{
    std::string text;

    bool operator==(const RawJsonText& rhs) const = default;
};

NX_REFLECT_API DeserializationResult deserialize(const DeserializationContext& ctx, RawJsonText* data);

NX_REFLECT_API void serialize(SerializationContext* ctx, const RawJsonText& data);

} // namespace nx::reflect::json
