// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/serialization/format.h>

namespace nx::vms::common::detail {

/**
 * Wraps nx_fusion serialization/deserialization to provide common API with nx_reflect.
 * This is needed while nx_reflect and nx_fusion are both used in the codebase.
 */
class NxFusionWrapper
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

            default:
                return std::make_tuple(nx::Buffer(), false);
        }
    }

    template<typename T>
    static std::tuple<nx::Buffer, bool /*result*/> serializeToJson(const T& data)
    {
        return std::make_tuple(
            nx::Buffer(QJson::serialized(data)), true);
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

            default:
                return false;
        }
    }

    template<class T>
    static bool deserializeFromJson(const nx::Buffer& data, T* const target)
    {
        return QJson::deserialize(data, target);
    }
};

} // namespace nx::vms::common::detail
