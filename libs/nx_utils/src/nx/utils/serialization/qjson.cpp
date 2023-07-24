// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qjson.h"

#include <nx/utils/log/assert.h>

void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const QJsonObject& data)
{
    ctx->composer.writeRawString(
        QJsonDocument(data).toJson(QJsonDocument::Compact).toStdString());
}

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
