// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <unordered_map>

#include "raw_json_text.h"

namespace nx::reflect::json {

namespace detail {

/**
 * Regular JSON object.
 * It extends std::unordered_map by adding a way to use any reflectable type as value.
 * Note: inheriting std::unordered_map is needed to make the class reflectable since
 * NX_REFLECTION_INSTRUMENT macro requires a type with strictly defined fields.
 */
class NX_REFLECT_API Object:
    public std::unordered_map<std::string, RawJsonText>
{
public:
    /**
     * T MUST be deserialize-able from JSON.
     */
    template<typename T>
    std::optional<T> get(const std::string& attrName) const
    {
        if (!contains(attrName))
            return std::nullopt;

        auto [data, result] = nx::reflect::json::deserialize<T>(at(attrName).jsonText);
        if (!result.success)
            return std::nullopt;
        return std::make_optional(std::move(data));
    }

    /**
     * T MUST be serialize-able to JSON.
     * If attribute with the given name already exists, it will be overwritten.
     */
    template<typename T>
    void set(const std::string& attrName, const T& value)
    {
        (*this)[attrName].jsonText = nx::reflect::json::serialize(value);
    }
};

} // namespace detail

using Object = detail::Object;

} // namespace nx::reflect::json
