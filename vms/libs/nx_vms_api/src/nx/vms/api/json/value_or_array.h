// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <QJsonValue>

class QnJsonContext;

namespace nx::vms::api::json {

template<typename T>
struct ValueOrArray
{
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
        return deserialize(ctx, value, &target->valueOrArray);
    T data;
    if (!deserialize(ctx, value, &data))
        return false;
    target->valueOrArray.resize(1);
    target->valueOrArray[0] = data;
    return true;
}

} // namespace nx::vms::api::json
