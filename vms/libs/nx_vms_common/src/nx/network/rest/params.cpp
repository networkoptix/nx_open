// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "params.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/fusion/serialization/json.h>
#include <nx/network/rest/exception.h>

namespace nx::network::rest {

namespace {

struct ApiErrorStrings
{
    Q_DECLARE_TR_FUNCTIONS(ApiErrorStrings);
};

} // namespace

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
        params.insert(it.key(), valueToString(std::move(it.value())));

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

// TODO: Fix or refactor. This function disregards possible content errors. For example,
//     invalid JSON content (parsing errors) will be silently ignored.
//     For details: VMS-41649
std::optional<QJsonValue> Content::parse() const
{
    try
    {
        return parseOrThrow();
    }
    catch (const rest::Exception& e)
    {
        NX_DEBUG(this, "Content parsing error: \"%1\"", e.what());
        return std::nullopt;
    }
}

QJsonValue Content::parseOrThrow() const
{
    if (type == http::header::ContentType::kForm)
        return Params::fromUrlQuery(QUrlQuery(body)).toJson();

    if (type == http::header::ContentType::kJson)
    {
        QJsonValue value;
        if (!QJsonDetail::deserialize_json(body, &value))
            throw Exception::badRequest(ApiErrorStrings::tr("Invalid JSON content."));

        return value;
    }

    // TODO: Other content types should go there when supported.
    // TODO: `Exception::unsupportedContentType`
    throw Exception::unsupportedMediaType(ApiErrorStrings::tr("Unsupported content type."));
}

} // namespace nx::network::rest
