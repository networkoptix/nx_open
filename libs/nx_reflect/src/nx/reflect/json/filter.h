// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../filter.h"
#include "serializer.h"

namespace nx::reflect::json {

struct Matcher
{
    static inline bool isJsonString(const std::string& json)
    {
        return json.size() >= 2 && json[0] == '"';
    }

    template<typename T>
    static bool matches(const T& data, const std::vector<std::string>& possibleValues)
    {
        const auto json{serialize(data)};
        return std::any_of(possibleValues.begin(), possibleValues.end(),
            [&json, isDataJsonString = isJsonString(json)](const auto& v)
            {
                return isDataJsonString && !isJsonString(v)
                    ? std::string_view{json.data() + 1, json.size() - 2} == v
                    : json == v;
            });
    }

    static bool matches(const std::string& value, const std::vector<std::string>& possibleValues)
    {
        return std::any_of(possibleValues.begin(), possibleValues.end(),
            [&value](const auto& v) { return value == v; });
    }
};

template<typename T>
bool filter(T* data, const Filter& filter)
{
    return nx::reflect::filter<Matcher>(data, filter);
}

} // namespace nx::reflect::json
