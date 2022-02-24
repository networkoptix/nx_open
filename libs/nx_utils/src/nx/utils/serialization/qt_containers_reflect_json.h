// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Custom nx_reflect/json functions for Qt containers.
 *
 * Qt associative containers do not satisfy STL concepts like AssociativeContainer. So, separate
 * support functions are added here. Qt sequence containers (QList, QVector) work out of the box.
 */

#include <QtCore/QMap>
#include <QtCore/QSet>

#include <nx/reflect/json.h>

//-------------------------------------------------------------------------------------------------
// QSet

template<typename T>
void serialize(
    nx::reflect::json::SerializationContext* context,
    const QSet<T>& value)
{
    context->composer.startArray();
    for (const auto& element: value)
        nx::reflect::json::serialize(context, element);
    context->composer.endArray();
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
struct IsQtAssociativeContainer<QHash<Args...>> { static constexpr bool value = true; };

template<typename... Args>
inline constexpr bool IsQtAssociativeContainerV = IsQtAssociativeContainer<Args...>::value;

template<typename Container>
requires IsQtAssociativeContainerV<Container>
void serialize(
    nx::reflect::json::SerializationContext* context,
    const Container& value)
{
    context->composer.startObject();
    for (auto it = value.begin(); it != value.end(); ++it)
    {
        // NOTE: Supporting only string keys.
        context->composer.writeAttributeName(nx::reflect::toString(it.key()));
        nx::reflect::json::serialize(context, it.value());
    }
    context->composer.endObject();
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
