// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonValue>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/dot_notation_string.h>
#include <nx/utils/serialization/json.h>

#include "params.h"

namespace std {

template<>
struct less<QJsonValue>
{
    bool operator()(const QJsonValue& lhs, const QJsonValue& rhs) const
    {
        return QJson::serialized(lhs) < QJson::serialized(rhs);
    }
};

template<>
struct less<QJsonObject>
{
    bool operator()(const QJsonObject& lhs, const QJsonObject& rhs) const
    {
        return QJson::serialized(lhs) < QJson::serialized(rhs);
    }
};

} // namespace std

namespace nx::network::rest::json {

enum class DefaultValueAction: bool
{
    removeEqual,
    appendMissing,
};

namespace details {

NX_NETWORK_REST_API bool merge(
    QJsonValue* existingValue,
    const QJsonValue& incompleteValue,
    QString* outErrorMessage,
    const QString& fieldName);

NX_NETWORK_REST_API void filter(
    QJsonValue* value,
    const QJsonValue* defaultValue,
    DefaultValueAction action,
    Params filters = {},
    const nx::utils::DotNotationString& with = {});

NX_NETWORK_REST_API nx::utils::DotNotationString extractWithParam(Params* filters);
NX_NETWORK_REST_API void filter(
    rapidjson::Document* value, const nx::utils::DotNotationString& with);

} // namespace details

template<typename Output, typename Input>
bool merge(
    Output* requestedValue,
    const Input& existingValue,
    const QJsonValue& incompleteValue,
    QString* outErrorMessage,
    bool chronoSerializedAsDouble)
{
    QnJsonContext jsonContext;
    jsonContext.setChronoSerializedAsDouble(chronoSerializedAsDouble);

    QJsonValue jsonValue;
    QJson::serialize(&jsonContext, existingValue, &jsonValue);
    if (!details::merge(&jsonValue, incompleteValue, outErrorMessage, QString()))
        return false;

    jsonContext.setStrictMode(true);
    jsonContext.deserializeReplacesExistingOptional(false);
    if (QJson::deserialize(&jsonContext, jsonValue, requestedValue))
        return true;

    *outErrorMessage = "Unable to deserialize merged Json data to destination object.";
    return false;
}

template<typename T>
inline QJsonValue defaultValue()
{
    QJsonValue value;
    QnJsonContext jsonContext;
    jsonContext.setSerializeMapToObject(true);
    jsonContext.setOptionalDefaultSerialization(true);
    jsonContext.setChronoSerializedAsDouble(true);
    QJson::serialize(&jsonContext, T(), &value);
    return value;
}

template<typename T>
inline QJsonValue serialized(const T& data)
{
    QnJsonContext jsonContext;
    jsonContext.setSerializeMapToObject(true);
    jsonContext.setChronoSerializedAsDouble(true);
    QJsonValue value;
    QJson::serialize(&jsonContext, data, &value);
    return value;
}

// TODO: Switch on nx_reflect and remove.
NX_NETWORK_REST_API QJsonValue serialized(rapidjson::Value& data);

// throws exception if can't process all withs
template<typename T>
QJsonValue filter(
    const T& data, Params filters = {},
    DefaultValueAction defaultValueAction = DefaultValueAction::appendMissing)
{
    auto value = serialized(data);
    auto with = details::extractWithParam(&filters);
    static const auto defaultJson = defaultValue<T>();
    details::filter(&value, &defaultJson, defaultValueAction, std::move(filters), std::move(with));
    return value;
}

template<typename T>
rapidjson::Document serialize(const T& data, Params params, DefaultValueAction defaultValueAction)
{
    // TODO: Validate with against data type.
    auto json = nx::utils::serialization::json::serialized(
        data, defaultValueAction == DefaultValueAction::removeEqual);
    details::filter(&json, details::extractWithParam(&params));
    return json;
}

NX_NETWORK_REST_API QByteArray serialized(
    const QJsonValue& value, Qn::SerializationFormat format);

NX_NETWORK_REST_API QByteArray serialized(
    const rapidjson::Value& value, Qn::SerializationFormat format);

} // namespace nx::network::rest::json
