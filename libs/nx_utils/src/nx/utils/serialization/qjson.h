// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <nx/reflect/json/serializer.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/utils/log/assert.h>

namespace nx::reflect::json_detail {

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


void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const QJsonValue& data)
{
    if (data.isArray())
    {
        ctx->composer.startArray();
        const auto& arr = data.toArray();
        for (const auto& item: arr)
            nx::reflect::json::serialize(ctx, item);
        ctx->composer.endArray();
        return;
    }
    if (data.isObject())
    {
        ctx->composer.writeRawString(
            QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact).toStdString());
        return;
    }
    if (data.isString())
    {
        ctx->composer.writeString(data.toString().toStdString());
        return;
    }
    if (data.isBool())
    {
        ctx->composer.writeBool(data.toBool());
        return;
    }
    if (data.isDouble())
    {
        ctx->composer.writeFloat(data.toDouble());
        return;
    }
    if (data.isNull())
    {
        ctx->composer.writeNull();
        return;
    }
    NX_ASSERT(false);
    return;
}

void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const QJsonValueConstRef& data)
{
    serialize(ctx, (QJsonValue) data);
}
