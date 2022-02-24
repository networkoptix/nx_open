// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QJsonArray>
#include <QJsonObject>

#include "csv.h"

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
        const auto value = it.value();
        if (value.isObject())
            collectHeader(value, &fields->nested[it.key()]);
        else if (!value.isArray())
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

} // namespace QnCsvDetail
