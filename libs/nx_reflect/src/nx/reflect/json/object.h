// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <unordered_map>

#include "raw_json_text.h"

namespace nx::reflect::json {

namespace detail {

/**
 * Regular JSON object.
 */
class NX_REFLECT_API Object
{
    friend NX_REFLECT_API DeserializationResult deserialize(const DeserializationContext& ctx, Object* data);
    friend NX_REFLECT_API void serialize(SerializationContext* ctx, const Object& data);

public:
    bool contains(const std::string& attrName) const;

    /**
     * T MUST be deserialize-able from JSON.
     */
    template<typename T>
    std::optional<T> get(const std::string& attrName) const
    {
        if (!m_attrs.contains(attrName))
            return std::nullopt;

        auto [data, result] = nx::reflect::json::deserialize<T>(m_attrs.at(attrName).text);
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
        m_attrs[attrName].text = nx::reflect::json::serialize(value);
    }

    bool operator==(const Object& rhs) const = default;

private:
    std::unordered_map<std::string, RawJsonText> m_attrs;
};

NX_REFLECT_API DeserializationResult deserialize(const DeserializationContext& ctx, Object* data);
NX_REFLECT_API void serialize(SerializationContext* ctx, const Object& data);

} // namespace detail

using Object = detail::Object;

} // namespace nx::reflect::json
