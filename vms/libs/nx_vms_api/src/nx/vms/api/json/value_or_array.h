// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QJsonValue>
#include <vector>

#include <nx/reflect/json/deserializer.h>
#include <nx/utils/log/assert.h>

class QnJsonContext;

namespace nx::vms::api::json {

template<typename T>
struct ValueOrArray
{
    ValueOrArray() = default;

    ValueOrArray(T value)
    {
        valueOrArray.push_back(std::move(value));
    }

    ValueOrArray(std::initializer_list<T> init):
        valueOrArray(std::move(init))
    {}

    ValueOrArray(const std::vector<T>& vector):
        valueOrArray(vector)
    {}

    bool empty() const { return valueOrArray.empty(); }

    bool operator==(const ValueOrArray& origin) const { return valueOrArray == origin.valueOrArray; }

    /**%apidoc[unused] */
    std::vector<T> valueOrArray;
};

template<typename T>
inline void serialize(QnJsonContext* ctx, const ValueOrArray<T>& value, QJsonValue* target)
{
    if (value.valueOrArray.size() == 1)
        serialize(ctx, value.valueOrArray[0], target);
    else
        serialize(ctx, value.valueOrArray, target);
}

template<typename T>
inline bool deserialize(QnJsonContext* ctx, const QJsonValue& value, ValueOrArray<T>* target)
{
    if (value.isArray())
        return deserialize(ctx, value, &target->valueOrArray) && !target->valueOrArray.empty();
    T data;
    if (!deserialize(ctx, value, &data))
        return false;
    target->valueOrArray.resize(1);
    target->valueOrArray[0] = data;
    return true;
}

template<typename T>
nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& context, ValueOrArray<T>* data)
{
    if (context.value.IsArray())
    {
        data->valueOrArray.reserve(data->valueOrArray.size() + context.value.Size());
        if (data->valueOrArray.empty())
            return nx::reflect::json::deserialize(context, &data->valueOrArray);

        std::vector<T> list;
        auto r = nx::reflect::json::deserialize(context, &list);
        if (r)
        {
            for (auto&& v: list)
                data->valueOrArray.emplace_back(std::move(v));
        }
        return r;
    }

    T value;
    auto r = nx::reflect::json::deserialize(context, &value);
    if (r)
    {
        data->valueOrArray.reserve(data->valueOrArray.size() + 1);
        data->valueOrArray.emplace_back(std::move(value));
    }
    return r;
}

} // namespace nx::vms::api::json
