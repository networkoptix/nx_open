// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/reflect/json/deserializer.h>

class QJsonValue;
class QnJsonContext;

namespace nx::json {

struct Int64AsNumber
{
    /**%apidoc[unused] */
    int64_t value;

    Int64AsNumber(const Int64AsNumber&) = default;
    Int64AsNumber(Int64AsNumber&&) = default;
    Int64AsNumber& operator=(const Int64AsNumber&) = default;
    Int64AsNumber& operator=(Int64AsNumber&&) = default;

    Int64AsNumber(int64_t other = 0): value(other) {}
    Int64AsNumber& operator=(int64_t other)
    {
        value = other;
        return *this;
    }

    operator int64_t() const { return value; }

    Int64AsNumber& operator++()
    {
        ++value;
        return *this;
    }

    Int64AsNumber operator++(int)
    {
        Int64AsNumber r = *this;
        ++value;
        return r;
    }

    QString toString() const { return QString::number(value); }
};

inline nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& context, Int64AsNumber* data)
{
    if (context.value.IsInt64())
    {
        *data = context.value.GetInt64();
        return {};
    }

    if (context.value.IsInt())
    {
        *data = context.value.GetInt();
        return {};
    }

    if constexpr (sizeof(std::decay_t<decltype(context.value.GetUint())>) < 8u)
    {
        if (context.value.IsUint())
        {
            *data = context.value.GetUint();
            return {};
        }
    }

    return {false, "Signed 64 bit integer expected",
        nx::reflect::json_detail::getStringRepresentation(context.value)};
}

template<typename SerializationContext>
inline void serialize(SerializationContext* context, const Int64AsNumber& value)
{
    context->composer.writeInt(value.value);
}

NX_UTILS_API void serialize(QnJsonContext*, const Int64AsNumber& value, QJsonValue* out);
NX_UTILS_API bool deserialize(QnJsonContext*, const QJsonValue& value, Int64AsNumber* out);

} // namespace nx::json

template<>
struct std::hash<nx::json::Int64AsNumber>
{
    size_t operator()(const nx::json::Int64AsNumber& value) const
    {
        return std::hash<int64_t>()(value.value);
    }
};
