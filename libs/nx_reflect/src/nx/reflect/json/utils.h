// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <tuple>

#include <rapidjson/document.h>

#include <nx/reflect/serialization_utils.h>

namespace nx::reflect::json_detail {

NX_REFLECT_API std::string getStringRepresentation(const rapidjson::Value& val);
NX_REFLECT_API std::string parseErrorToString(const rapidjson::ParseResult& parseResult);

} // namespace nx::reflect::json_detail

//--------------------------------------------------------------------------------------------------

namespace nx::reflect::json {

/**
 * Deserializes JSON and serializes it back to omit extra spaces, tabs, line feeds.
 */
NX_REFLECT_API std::tuple<std::string, DeserializationResult>
    compactJson(const std::string_view json);

} // namespace nx::reflect::json
