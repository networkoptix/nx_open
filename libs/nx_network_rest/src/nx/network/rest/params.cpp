// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "params.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/fusion/serialization/json.h>

namespace nx::network::rest {

Params::Params(const QMultiMap<QString, QString>& map):
    m_values(map)
{
}

Params::Params(const QHash<QString, QString>& hash)
{
    for (auto it = hash.begin(); it != hash.end(); ++it)
        insert(it.key(), it.value());
}

Params::Params(std::initializer_list<std::pair<QString, QString>> list)
{
    for (auto it = list.begin(); it != list.end(); ++it)
        insert(it->first, it->second);
}

Params Params::fromUrlQuery(const QUrlQuery& query)
{
    return fromList(query.queryItems(QUrl::FullyDecoded));
}

Params Params::fromList(const QList<QPair<QString, QString>>& list)
{
    Params params;
    for (const auto& item: list)
        params.insert(item.first, item.second);

    return params;
}

Params Params::fromJson(const QJsonObject& value)
{
    Params params;
    for (auto it = value.begin(); it != value.end(); ++it)
    {
        if (it->isArray())
        {
            for (const auto& arrayItem: it->toArray())
                params.insert(it.key(), valueToString(arrayItem));
        }
        else
        {
            params.insert(it.key(), valueToString(it.value()));
        }
    }

    return params;
}

QString Params::valueToString(QJsonValue value)
{
    static const QString kTrue("true");
    static const QString kFalse("false");
    switch (value.type())
    {
        case QJsonValue::Null:
            return QString();
        case QJsonValue::Bool:
            return value.toBool() ? kTrue : kFalse;
        case QJsonValue::Double:
            return QString::number(value.toDouble());
        case QJsonValue::Array:
            return QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact);
        case QJsonValue::Object:
            return QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
        default:
            return value.toString();
    }
    return {};
}

QUrlQuery Params::toUrlQuery() const
{
    QUrlQuery query;
    query.setQueryItems(toList());
    return query;
}

QList<QPair<QString, QString>> Params::toList() const
{
    QList<QPair<QString, QString>> list;
    for (auto it = m_values.begin(); it != m_values.end(); ++it)
        list.append({it.key(), it.value()});

    return list;
}

QMultiMap<QString, QString> Params::toMap() const
{
    return m_values;
}

bool Params::hasNonRefParameter() const
{
    for (auto it = m_values.begin(); it != m_values.end(); ++it)
    {
        if (!it.key().startsWith(QChar('_')))
            return true;
    }
    return false;
}

QJsonObject Params::toJson(bool excludeCommon) const
{
    QJsonObject object;
    if (m_values.isEmpty())
        return object;

    auto it = m_values.begin();
    QString key = it.key();
    QJsonArray values;
    values.append(QJsonValue(it.value()));
    for (++it; it != m_values.end(); ++it)
    {
        if (key == it.key())
        {
            values.append(QJsonValue(it.value()));
            continue;
        }
        if (!excludeCommon || !key.startsWith(QChar('_')))
            object.insert(key, (values.size() == 1) ? values[0] : QJsonValue(values));
        values = QJsonArray();
        values.append(QJsonValue(it.value()));
        key = it.key();
    }
    if (!excludeCommon || !key.startsWith(QChar('_')))
        object.insert(key, (values.size() == 1) ? values[0] : QJsonValue(values));
    return object;
}

void Params::rename(const QString& oldName, const QString& newName)
{
    for (auto it = m_values.find(oldName); it != m_values.end(); it = m_values.find(oldName))
    {
        auto value = *it;
        m_values.erase(it);
        m_values.insert(newName, value);
    }
}

Params Params::fromJson(const rapidjson::Value& value)
{
    Params params;
    for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it)
        params.insert(it->name.GetString(), valueToString(it->value));

    return params;
}

QString Params::valueToString(const rapidjson::Value& value)
{
    static const QString kTrue("true");
    static const QString kFalse("false");
    switch (value.GetType())
    {
        case rapidjson::kNullType:
            return QString();
        case rapidjson::kFalseType:
            return kFalse;
        case rapidjson::kTrueType:
            return kTrue;
        case rapidjson::kNumberType:
            return QString::number(value.GetDouble());
        case rapidjson::kStringType:
            return value.GetString();
        case rapidjson::kArrayType:
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
            NX_ASSERT(value.Accept(writer), "Failed to stringify JSON array");
            return buffer.GetString();
        }
        case rapidjson::kObjectType:
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
            NX_ASSERT(value.Accept(writer), "Failed to stringify JSON object");
            return buffer.GetString();
        }
    }
    return {};
}

} // namespace nx::network::rest
