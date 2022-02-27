// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <rapidjson/document.h>

namespace nx::reflect::json_detail {

NX_REFLECT_API std::string getStringRepresentation(const rapidjson::Value& val);

} // namespace nx::reflect::json_detail
