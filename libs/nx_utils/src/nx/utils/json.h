// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/assert.h>

#include <QJsonArray>
#include <QJsonObject>

namespace nx {
namespace utils {
namespace json {

inline QJsonObject asObject(const QJsonValue& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.isObject(), "%1 is not an object (%2)", value, assertMessage);
    return value.toObject();
}

inline QJsonArray asArray(const QJsonValue& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.isArray(), "%1 is not an array (%2)", value, assertMessage);
    return value.toArray();
}

inline QString asString(const QJsonValue& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.isString(), "%1 is not a string (%2)", value, assertMessage);
    return value.toString();
}

inline double asDouble(const QJsonValue& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.isDouble(), "%1 is not a number (%2)", value, assertMessage);
    return value.toDouble();
}

inline bool asBool(const QJsonValue& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.isBool(), "%1 is not a bool (%2)", value, assertMessage);
    return value.toBool();
}

inline QJsonValue getField(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    if (const auto it = object.find(key); it != object.end())
    {
        return it.value();
    }

    NX_ASSERT(false, "No field '%1' in %2 (%3)", key, object, assertMessage);
    return QJsonValue();
}

inline QJsonObject getObject(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    return asObject(getField(object, key, assertMessage), assertMessage);
}

inline QJsonObject getObject(
    const QJsonValue& value, const QString& key, const QString& assertMessage = {})
{
    return getObject(asObject(value, assertMessage), key, assertMessage);
}

inline QJsonArray getArray(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    return asArray(getField(object, key, assertMessage), assertMessage);
}

inline QJsonArray getArray(
    const QJsonValue& value, const QString& key, const QString& assertMessage = {})
{
    return getArray(asObject(value, assertMessage), key, assertMessage);
}

inline QString getString(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    return asString(getField(object, key, assertMessage), assertMessage);
}

inline QString getString(
    const QJsonValue& value, const QString& key, const QString& assertMessage = {})
{
    return getString(asObject(value, assertMessage), key, assertMessage);
}

inline double getDouble(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    return asDouble(getField(object, key, assertMessage), assertMessage);
}

inline double getDouble(
    const QJsonValue& value, const QString& key, const QString& assertMessage = {})
{
    return getDouble(asObject(value, assertMessage), key, assertMessage);
}

inline bool getBool(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    return asBool(getField(object, key, assertMessage), assertMessage);
}

inline bool getBool(
    const QJsonValue& value, const QString& key, const QString& assertMessage = {})
{
    return getBool(asObject(value, assertMessage), key, assertMessage);
}

inline QJsonObject optObject(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    const auto it = object.find(key);
    return it != object.end() ? asObject(it.value(), assertMessage) : QJsonObject();
}

inline QJsonObject optObject(
    const QJsonValue& value, const QString& key, const QString& assertMessage = {})
{
    return optObject(asObject(value, assertMessage), key, assertMessage);
}

inline QJsonArray optArray(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    const auto it = object.find(key);
    return it != object.end() ? asArray(it.value(), assertMessage) : QJsonArray();
}

inline QJsonArray optArray(
    const QJsonValue& value, const QString& key, const QString& assertMessage = {})
{
    return optArray(asObject(value, assertMessage), key, assertMessage);
}

inline QString optString(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    const auto it = object.find(key);
    return it != object.end() ? asString(it.value(), assertMessage) : QString();
}

inline QString optString(
    const QJsonValue& value, const QString& key, const QString& assertMessage = {})
{
    return optString(asObject(value, assertMessage), key, assertMessage);
}

inline bool optBool(
    const QJsonObject& object, const QString& key, const QString& assertMessage = {})
{
    const auto it = object.find(key);
    return it != object.end() ? asBool(it.value(), assertMessage) : false;
}

inline bool optBool(const QJsonValue& value, const QString& key, const QString& assertMessage = {})
{
    return optBool(asObject(value, assertMessage), key, assertMessage);
}

} // namespace json
} // namespace utils
} // namespace nx
