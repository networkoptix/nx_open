// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sql_functions.h"

#include "json.h"

void serialize_field(const std::vector<nx::Uuid>&value, QVariant *target)
{
    QByteArray result;
    for (const auto& id: value)
        result.append(id.toRfc4122());
    serialize_field(result, target);
}

void deserialize_field(const QVariant &value, std::vector<nx::Uuid> *target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    NX_ASSERT(tmp.size() % 16 == 0);
    const char* data = tmp.data();
    const char* dataEnd = data + tmp.size();
    for(; data < dataEnd; data += 16)
        target->push_back(nx::Uuid::fromRfc4122(QByteArray::fromRawData(data, 16)));
}

void serialize_field(const QJsonValue& value, QVariant* target)
{
    QByteArray tmp;
    QJsonDetail::serialize_json(value, &tmp);
    serialize_field(tmp, target);
}

void deserialize_field(const QVariant& value, QJsonValue* target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    QString errorMessage;
    NX_ASSERT(QJsonDetail::deserialize_json(tmp, target, &errorMessage), errorMessage);
}
