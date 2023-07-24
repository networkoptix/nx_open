// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>

namespace nx::reflect::json_detail {

template<typename T>
DeserializationResult deserialize(
    const DeserializationContext& ctx,
    T* data,
    std::enable_if_t<std::is_same_v<T, QJsonObject>>* = nullptr)
{
    const auto& val = ctx.value;
    if (!val.IsObject())
    {
        return DeserializationResult(
            false, "Can't parse a QJson object", getStringRepresentation(val));
    }

    auto qStrVal = QString::fromStdString(getStringRepresentation(val));
    QJsonParseError error;
    auto qjsonDoc = QJsonDocument::fromJson(qStrVal.toUtf8(), &error);
    if (qjsonDoc.isNull())
    {
        return DeserializationResult(
            false, error.errorString().toStdString(), getStringRepresentation(val));
    }

    *data = qjsonDoc.object();
    return DeserializationResult(true);
}

template<typename T>
DeserializationResult deserialize(
    const DeserializationContext& ctx,
    T* data,
    std::enable_if_t<std::is_same_v<T, QJsonValue>>* = nullptr)
{
    const auto& val = ctx.value;
    if (val.IsObject() || val.IsArray())
    {
        auto qStrVal = QString::fromStdString(getStringRepresentation(val));
        QJsonParseError error;
        auto qjsonDoc = QJsonDocument::fromJson(qStrVal.toUtf8(), &error);
        if (qjsonDoc.isNull())
            return DeserializationResult(
                false, error.errorString().toStdString(), getStringRepresentation(val));
        if (val.IsObject())
            *data = qjsonDoc.object();
        if (val.IsArray())
            *data = qjsonDoc.array();
        return DeserializationResult(true);
    }
    if (val.IsString())
    {
        *data = QJsonValue(val.GetString());
        return DeserializationResult(true);
    }
    if (val.IsBool())
    {
        *data = QJsonValue(val.GetBool());
        return DeserializationResult(true);
    }
    if (val.IsNumber())
    {
        *data = QJsonValue(val.GetDouble());
        return DeserializationResult(true);
    }
    if (val.IsNull())
    {
        *data = QJsonValue();
        return DeserializationResult(true);
    }

    return DeserializationResult(false, "Can't parse a QJson value", getStringRepresentation(val));
}

} // namespace nx::reflect::json_detail

NX_UTILS_API void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const QJsonObject& data);

NX_UTILS_API void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const QJsonValue& data);

NX_UTILS_API void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const QJsonValueConstRef& data);
