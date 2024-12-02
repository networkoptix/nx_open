// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <rapidjson/document.h>

#include <nx/reflect/json/utils.h>
#include <nx/utils/log/assert.h>

namespace nx::utils::reflect {

// rapidjson::Value from rapidjson::Document DOM are not allocated using usual memory allocator.
// They are allocated on chunks of MemoryPoolAllocator that doesn't free them on removing. Object,
// array and string modifications are executed with the same allocator. That way all the pointers
// that are returned from a rapidjson::Value are valid until their allocator lives in their
// rapidjson::Document used to parse the DOM.

inline std::string toString(const rapidjson::Value& value)
{
    return nx::reflect::json_detail::getStringRepresentation(value);
}

inline std::string asString(const rapidjson::Value::StringRefType& value)
{
    return {value.s, value.length};
}

inline rapidjson::Value::StringRefType asStringRef(
    const rapidjson::Value& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.IsString(), "%1 is not a string (%2)", toString(value), assertMessage);
    return {value.GetString(), value.GetStringLength()};
}

inline std::string asString(const rapidjson::Value& value, const QString& assertMessage = {})
{
    return asString(asStringRef(value, assertMessage));
}

inline rapidjson::Value::Object asObject(rapidjson::Value& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.IsObject(), "%1 is not an object (%2)", toString(value), assertMessage);
    return value.GetObject();
}

inline rapidjson::Value::Array asArray(rapidjson::Value& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.IsArray(), "%1 is not an array (%2)", toString(value), assertMessage);
    return value.GetArray();
}

inline double asDouble(const rapidjson::Value& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.IsNumber(), "%1 is not a number (%2)", toString(value), assertMessage);
    return value.GetDouble();
}

inline bool asBool(const rapidjson::Value& value, const QString& assertMessage = {})
{
    NX_ASSERT(value.IsBool(), "%1 is not a bool (%2)", toString(value), assertMessage);
    return value.GetBool();
}

inline rapidjson::Value& getField(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    if (auto it = object.FindMember(key); it != object.MemberEnd())
        return it->value;

    NX_ASSERT(false, "No field '%1' in %2 (%3)", key.s, toString(object), assertMessage);
    return object[key.s]; //< Use rapidjson::Value::Object adhoc.
}

inline rapidjson::Value::Object getObject(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return asObject(getField(object, std::move(key), assertMessage), assertMessage);
}

inline rapidjson::Value::Object getObject(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return getObject(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline rapidjson::Value::Array getArray(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return asArray(getField(object, std::move(key), assertMessage), assertMessage);
}

inline rapidjson::Value::Array getArray(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return getArray(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline rapidjson::Value::StringRefType getStringRef(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return asStringRef(getField(object, std::move(key), assertMessage), assertMessage);
}

inline rapidjson::Value::StringRefType getStringRef(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return getStringRef(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline std::string getString(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return asString(getField(object, std::move(key), assertMessage), assertMessage);
}

inline std::string getString(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return getString(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline double getDouble(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return asDouble(getField(object, std::move(key), assertMessage), assertMessage);
}

inline double getDouble(
    rapidjson::Value& value, rapidjson::Value::StringRefType key, const QString& assertMessage = {})
{
    return getDouble(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline bool getBool(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return asBool(getField(object, std::move(key), assertMessage), assertMessage);
}

inline bool getBool(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return getBool(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline rapidjson::Value::Object optObject(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    static rapidjson::Value empty(rapidjson::kObjectType);
    const auto it = object.FindMember(key);
    return it != object.MemberEnd() ? asObject(it->value, assertMessage) : empty.GetObject();
}

inline rapidjson::Value::Object optObject(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return optObject(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline rapidjson::Value::Array optArray(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    static rapidjson::Value empty(rapidjson::kArrayType);
    const auto it = object.FindMember(key);
    return it != object.MemberEnd() ? asArray(it->value, assertMessage) : empty.GetArray();
}

inline rapidjson::Value::Array optArray(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return optArray(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline rapidjson::Value::StringRefType optStringRef(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    const auto it = object.FindMember(key);
    return it != object.MemberEnd()
        ? asStringRef(it->value, assertMessage)
        : rapidjson::Value::StringRefType{nullptr, 0};
}

inline rapidjson::Value::StringRefType optStringRef(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return optStringRef(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline std::string optString(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    const auto it = object.FindMember(key);
    return it != object.MemberEnd() ? asString(it->value, assertMessage) : std::string{};
}

inline std::string optString(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return optString(asObject(value, assertMessage), std::move(key), assertMessage);
}

inline bool optBool(
    rapidjson::Value::Object object,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    const auto it = object.FindMember(key);
    return it != object.MemberEnd() ? asBool(it->value, assertMessage) : false;
}

inline bool optBool(
    rapidjson::Value& value,
    rapidjson::Value::StringRefType key,
    const QString& assertMessage = {})
{
    return optBool(asObject(value, assertMessage), std::move(key), assertMessage);
}

} // namespace nx::utils::reflect
