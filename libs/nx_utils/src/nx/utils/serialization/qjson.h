// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <nx/reflect/json.h>
#include <nx/utils/log/assert.h>

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
    if (val.IsInt64())
    {
        *data = QJsonValue((qint64) val.GetInt64());
        return DeserializationResult(true);
    }
    if (val.IsInt())
    {
        *data = QJsonValue(val.GetInt());
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

template<typename SerializationContext>
void serialize(SerializationContext* context, const QJsonObject& data);

template<typename SerializationContext>
void serialize(SerializationContext* context, const QJsonValueConstRef& data);

template<typename SerializationContext>
requires std::is_member_object_pointer_v<decltype(&SerializationContext::composer)>
void serialize(SerializationContext* context, const QJsonValue& data)
{
    if (data.isArray())
    {
        context->composer.startArray();
        const auto& arr = data.toArray();
        for (const auto& item: arr)
            serialize(context, item);
        context->composer.endArray(arr.size());
        return;
    }
    if (data.isObject())
    {
        serialize(context, data.toObject());
        return;
    }
    if (data.isString())
    {
        context->composer.writeString(data.toString().toStdString());
        return;
    }
    if (data.isBool())
    {
        context->composer.writeBool(data.toBool());
        return;
    }
    if (data.isDouble())
    {
        switch (data.toVariant().userType())
        {
            case QMetaType::Int:
            case QMetaType::LongLong:
                context->composer.writeInt(data.toDouble());
                return;
        }
        context->composer.writeFloat(data.toDouble());
        return;
    }
    if (data.isNull())
    {
        context->composer.writeNull();
        return;
    }
    NX_ASSERT(false, "Undefined QJsonValue");
}

template<typename SerializationContext>
void serialize(SerializationContext* context, const QJsonObject& data)
{
    context->composer.startObject();
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        context->composer.writeAttributeName(it.key().toStdString());
        serialize(context, it.value());
    }
    context->composer.endObject(data.size());
}

template<typename SerializationContext>
void serialize(SerializationContext* context, const QJsonValueConstRef& data)
{
    serialize(context, (QJsonValue) data);
}

template<typename SerializationContext>
void serialize(SerializationContext* context, const QVariantMap& value)
{
    serialize(context, QJsonObject::fromVariantMap(value));
}

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(QJsonValue::Type*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<QJsonValue::Type>;
    return visitor(
        Item{QJsonValue::Type::Null, "null"},
        Item{QJsonValue::Type::Bool, "boolean"},
        Item{QJsonValue::Type::Double, "number"},
        Item{QJsonValue::Type::String, "string"},
        Item{QJsonValue::Type::Array, "array"},
        Item{QJsonValue::Type::Object, "object"});
}
