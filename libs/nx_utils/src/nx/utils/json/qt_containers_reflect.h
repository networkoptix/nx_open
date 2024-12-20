// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Custom nx_reflect/json functions for Qt containers.
 *
 * Qt associative containers do not satisfy STL concepts like AssociativeContainer. So, separate
 * support functions are added here. Qt sequence containers (QList, QVector) work out of the box.
 */

#include <QtCore/QMap>
#include <QtCore/QMultiMap>
#include <QtCore/QSet>

#include <nx/reflect/array_orderer.h>
#include <nx/reflect/filter.h>
#include <nx/reflect/json.h>

#include "qt_core_types.h"

//-------------------------------------------------------------------------------------------------
// QSet

template<typename SerializationContext, typename T>
requires std::is_member_object_pointer_v<decltype(&SerializationContext::composer)>
void serialize(SerializationContext* context, const QSet<T>& value)
{
    context->composer.startArray();
    for (const auto& element: value)
        nx::reflect::BasicSerializer::serializeAdl(context, element);
    context->composer.endArray(value.size());
}

template<typename T>
nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& context,
    QSet<T>* value)
{
    using namespace nx::reflect;

    if (!context.value.IsArray())
        return false;

    for (int i = 0; i < (int) context.value.Size(); ++i)
    {
        T val;
        if (auto result = json::deserialize(json::DeserializationContext{context.value[i]}, &val);
            !result)
        {
            return result;
        }

        value->insert(std::move(val));
    }

    return DeserializationResult(true);
}

//-------------------------------------------------------------------------------------------------
// Qt associative containers (QMap/QHash)

template<typename T, typename... Args>
struct IsQtAssociativeContainer { static constexpr bool value = false; };

template<typename... Args>
struct IsQtAssociativeContainer<QMap<Args...>> { static constexpr bool value = true; };

template<typename... Args>
struct IsQtAssociativeContainer<QMultiMap<Args...>> { static constexpr bool value = true; };

template<typename... Args>
struct IsQtAssociativeContainer<QHash<Args...>> { static constexpr bool value = true; };

template<>
struct IsQtAssociativeContainer<QVariantMap>: std::false_type {};

template<typename... Args>
inline constexpr bool IsQtAssociativeContainerV = IsQtAssociativeContainer<Args...>::value;

template<typename SerializationContext, typename Container>
requires IsQtAssociativeContainerV<Container>
void serialize(SerializationContext* context, const Container& value)
{
    context->composer.startObject();
    for (auto it = value.begin(); it != value.end(); ++it)
    {
        // NOTE: Supporting only string keys.
        context->composer.writeAttributeName(nx::reflect::toString(it.key()));
        nx::reflect::BasicSerializer::serializeAdl(context, it.value());
    }
    context->composer.endObject(value.size());
}

template<template<typename...> typename Container, typename Key, typename Value>
requires IsQtAssociativeContainerV<Container<Key, Value>>
nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& context,
    Container<Key, Value>* value)
{
    using namespace nx::reflect;

    if (!context.value.IsObject())
        return false;

    for (auto it = context.value.MemberBegin(); it != context.value.MemberEnd(); ++it)
    {
        std::string_view keyStr(it->name.GetString(), it->name.GetStringLength());

        Key key;
        if (!fromString(keyStr, &key))
            return DeserializationResult(false, "QMap key must be a string", std::string(keyStr));

        Value val;
        if (auto result = json::deserialize(json::DeserializationContext{it->value}, &val); !result)
            return result;

        value->insert(std::move(key), std::move(val));
    }

    return DeserializationResult(true);
}

namespace nx::reflect {

template<typename Matcher, typename T>
bool filter(QSet<T>* data, const Filter& filter_)
{
    if (filter_.values.empty() && filter_.fields.empty())
        return false;

    if (!filter_.values.empty() && !Matcher::matches(*data, filter_.values))
        return true;

    erase_if(*data, [&filter_](auto& v) { return filter<Matcher>(&v, filter_); });
    return data->empty();
}

template<typename Matcher, template<typename...> typename Container, typename Key, typename Value>
requires IsQtAssociativeContainerV<Container<Key, Value>>
bool filter(Container<Key, Value>* data, const Filter& filter_)
{
    if (filter_.values.empty() && filter_.fields.empty())
        return false;

    if (!filter_.values.empty() && !Matcher::matches(*data, filter_.values))
        return true;

    if (data->empty())
        return false;

    for (const auto& field: filter_.fields)
    {
        if (field.name == "*")
        {
            erase_if(*data, [&field](auto& it) { return filter<Matcher>(&it.value(), field); });
            if (data->empty())
                return true;
        }
        else
        {
            auto key = fromString<Key>(field.name);
            auto it = data->find(key);
            if (it != data->end())
            {
                if (filter<Matcher>(&it.value(), field))
                {
                    data->erase(it);
                    if (data->empty())
                        return true;
                }
            }
        }
    }
    return data->empty();
}

template<template<typename...> typename Container, typename Key, typename Value>
requires IsQtAssociativeContainerV<Container<Key, Value>>
void order(Container<Key, Value>* data, const ArrayOrder& order_)
{
    for (auto it = data->begin(); it != data->end(); ++it)
        order(&it.value(), order_);
}

namespace array_orderer_details {

template<>
inline std::function<int(const QVariantMap&, const QVariantMap&)> makeComparator(const ArrayOrder&)
{
    return {};
}

} // namespace array_orderer_details

} // namespace nx::reflect

namespace std {

template<template<typename...> typename Container, typename Key, typename Value>
requires IsQtAssociativeContainerV<Container<Key, Value>>
struct less<Container<Key, Value>>
{
    constexpr inline bool operator()(
        const Container<Key, Value>&, const Container<Key, Value>&) const
    {
        return false;
    }
};

} // namespace std
