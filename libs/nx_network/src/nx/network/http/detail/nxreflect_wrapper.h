// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <nx/reflect/json.h>
#include <nx/reflect/urlencoded.h>
#include <nx/utils/serialization/format.h>

namespace nx::network::http::detail {

/**
 * Wraps nx_reflect serialization/deserialization to provide common API with nx_fusion.
 * This is needed while nx_reflect and nx_fusion are both used in the codebase.
 */
class NxReflectWrapper
{
public:
    template<typename T>
    static std::tuple<nx::Buffer, bool /*result*/> serialize(
        Qn::SerializationFormat format, const T& data)
    {
        switch (format)
        {
            case Qn::SerializationFormat::json:
                return serializeToJson(data);

            case Qn::SerializationFormat::urlEncoded:
                return std::make_tuple(nx::Buffer(nx::reflect::urlencoded::serialize(data)), true);

            default:
                return std::make_tuple(nx::Buffer(), false);
        }
    }

    template<typename T>
    static std::tuple<nx::Buffer, bool /*result*/> serializeToJson(const T& data)
    {
        return std::make_tuple(
            nx::Buffer(nx::reflect::json::serialize(data)), true);
    }

    template<class T>
    static bool deserialize(
        Qn::SerializationFormat format,
        const nx::Buffer& data,
        T* const target)
    {
        switch (format)
        {
            case Qn::SerializationFormat::json:
                return deserializeFromJson(data, target);

            case Qn::SerializationFormat::urlEncoded:
                return nx::reflect::urlencoded::deserialize(
                    std::string_view(data.data(), data.size()), target);

            default:
                return false;
        }
    }

    template<class T>
    static bool deserializeFromJson(const nx::Buffer& data, T* const target)
    {
        return nx::reflect::json::deserialize(
            std::string_view(data.data(), data.size()), target);
    }
};

} // namespace nx::network::http::detail
