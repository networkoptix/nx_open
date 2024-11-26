// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <rapidjson/document.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <nx/utils/std/algorithm.h>

#include "csv.h"
#include "lexical.h"

namespace QnCsvDetail {

struct Fields
{
    QMap<QString, Fields> nested;
};

inline void collectHeader(const QJsonValue& value, Fields* fields)
{
    if (value.isArray())
    {
        const auto jsonArray = value.toArray();
        for (const auto item: jsonArray)
            collectHeader(item, fields);
        return;
    }
    if (!value.isObject())
        return;
    const auto object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        const auto field = it.value();
        if (field.isObject())
            collectHeader(field, &fields->nested[it.key()]);
        else if (!field.isArray())
            NX_ASSERT(fields->nested[it.key()].nested.isEmpty());
    }
}

template<class Output>
inline void writeHeader(
    const QString& name, const Fields& fields, QnCsvStreamWriter<Output>* stream, bool* needComma)
{
    for (auto it = fields.nested.begin(); it != fields.nested.end(); ++it)
    {
        const QString nestedName = name.isEmpty() ? it.key() : name + '.' + it.key();
        if (it.value().nested.isEmpty())
        {
            if (*needComma)
                stream->writeComma();
            *needComma = true;
            QnCsv::serialize(nestedName, stream);
        }
        else
        {
            writeHeader(nestedName, it.value(), stream, needComma);
        }
    }
}

template<class Output>
inline void writeCommas(const Fields& fields, QnCsvStreamWriter<Output>* stream, bool* needComma)
{
    if (fields.nested.isEmpty())
    {
        if (*needComma)
            stream->writeComma();
        *needComma = true;
    }
    else
    {
        for (auto field = fields.nested.begin(); field != fields.nested.end(); ++field)
            writeCommas(field.value(), stream, needComma);
    }
}

template<class Output>
inline void serialize(
    const QJsonObject& object,
    const Fields& fields,
    QnCsvStreamWriter<Output>* stream,
    bool* needComma);

template<class Output>
inline void serialize(
    const QJsonValue& value,
    const Fields& fields,
    QnCsvStreamWriter<Output>* stream,
    bool* needComma)
{
    if (!value.isObject() && !value.isArray())
    {
        if (*needComma)
            stream->writeComma();
        *needComma = true;
    }
    switch (value.type())
    {
        case QJsonValue::Array:
            break;
        case QJsonValue::Bool:
            QnCsv::serialize(value.toBool(), stream);
            break;
        case QJsonValue::Double:
            QnCsv::serialize(value.toDouble(), stream);
            break;
        case QJsonValue::Object:
            serialize(value.toObject(), fields, stream, needComma);
            break;
        default:
            QnCsv::serialize(QnLexical::serialized(value), stream);
            break;
    }
}

template<class Output>
inline void serialize(
    const QJsonArray& array, const Fields& fields, QnCsvStreamWriter<Output>* stream)
{
    for (auto it = array.begin(); it != array.end(); ++it)
    {
        bool needComma = false;
        serialize(*it, fields, stream, &needComma);
        stream->writeEndline();
    }
}

template<class Output>
inline void serialize(
    const QJsonObject& object,
    const Fields& fields,
    QnCsvStreamWriter<Output>* stream,
    bool* needComma)
{
    auto field = fields.nested.begin();
    auto it = object.begin();
    while (field != fields.nested.end())
    {
        if (it != object.end())
        {
            const auto compare = it.key().compare(field.key());
            if (compare == 0)
            {
                serialize(it.value(), field.value(), stream, needComma);
                ++field;
            }
            if (compare <= 0)
            {
                ++it;
                continue;
            }
        }
        writeCommas(field.value(), stream, needComma);
        ++field;
    }
}

struct ReflectFields
{
    std::list<std::pair<const rapidjson::Value*, ReflectFields>> nested;
};

inline void collectHeader(const rapidjson::Value& value, ReflectFields* fields)
{
    if (value.IsArray())
    {
        for (auto it = value.Begin(); it != value.End(); ++it)
            collectHeader(*it, fields);
        return;
    }

    if (!value.IsObject())
        return;

    for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it)
    {
        auto nested = nx::utils::find_if(
            fields->nested, [&it](const auto& item) { return *item.first == it->name; });
        if (it->value.IsObject())
        {
            if (!nested)
                nested = &fields->nested.emplace_back(&it->name, ReflectFields{});
            collectHeader(it->value, &nested->second);
        }
        else if (!it->value.IsArray())
        {
            if (!nested)
                nested = &fields->nested.emplace_back(&it->name, ReflectFields{});
            NX_ASSERT(nested->second.nested.empty());
        }
    }
}

template<class Output>
inline void writeHeader(
    const std::string& name,
    const ReflectFields& fields,
    QnCsvStreamWriter<Output>* stream,
    bool* needComma)
{
    for (auto it = fields.nested.begin(); it != fields.nested.end(); ++it)
    {
        const std::string nestedName =
            name.empty() ? it->first->GetString() : name + '.' + it->first->GetString();
        if (it->second.nested.empty())
        {
            if (*needComma)
                stream->writeComma();
            *needComma = true;
            QnCsv::serialize(nestedName, stream);
        }
        else
        {
            writeHeader(nestedName, it->second, stream, needComma);
        }
    }
}

template<class Output>
inline void writeCommas(
    const ReflectFields& fields, QnCsvStreamWriter<Output>* stream, bool* needComma)
{
    if (fields.nested.empty())
    {
        if (*needComma)
            stream->writeComma();
        *needComma = true;
    }
    else
    {
        for (auto field = fields.nested.begin(); field != fields.nested.end(); ++field)
            writeCommas(field->second, stream, needComma);
    }
}

template<class Output>
inline void serialize(
    const rapidjson::Value& value,
    const ReflectFields& fields,
    QnCsvStreamWriter<Output>* stream,
    bool* needComma)
{
    if (!value.IsObject() && !value.IsArray())
    {
        if (*needComma)
            stream->writeComma();
        *needComma = true;
    }
    switch (value.GetType())
    {
        case rapidjson::kArrayType:
            break;
        case rapidjson::kFalseType:
        case rapidjson::kTrueType:
            QnCsv::serialize(value.GetBool(), stream);
            break;
        case rapidjson::kNumberType:
            QnCsv::serialize(value.GetDouble(), stream);
            break;
        case rapidjson::kObjectType:
        {
            auto it = value.MemberBegin();
            for (auto field = fields.nested.begin(); field != fields.nested.end();)
            {
                if (it == value.MemberEnd())
                {
                    for (; field != fields.nested.end(); ++field)
                        writeCommas(field->second, stream, needComma);
                    break;
                }
                if (it->value.IsArray())
                {
                    ++it;
                    continue;
                }
                if (*field->first == it->name)
                {
                    serialize(it->value, field->second, stream, needComma);
                    ++it;
                }
                else
                {
                    writeCommas(field->second, stream, needComma);
                }
                ++field;
            }
            break;
        }
        case rapidjson::kNullType:
            QnCsv::serialize(QByteArray{"null"}, stream);
            break;
        case rapidjson::kStringType:
            QnCsv::serialize(
                QString::fromUtf8(value.GetString(), value.GetStringLength()), stream);
            break;
    }
}

template<class Output>
inline void serialize(
    const rapidjson::Value& value, const ReflectFields& fields, QnCsvStreamWriter<Output>* stream)
{
    if (value.IsArray())
    {
        for (auto it = value.Begin(); it != value.End(); ++it)
        {
            bool needComma = false;
            serialize(*it, fields, stream, &needComma);
            stream->writeEndline();
        }
        return;
    }

    bool needComma = false;
    serialize(value, fields, stream, &needComma);
    stream->writeEndline();
}

} // namespace QnCsvDetail
