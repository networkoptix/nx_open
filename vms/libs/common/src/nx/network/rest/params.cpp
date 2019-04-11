#include "params.h"

namespace nx::network::rest {

Params::Params(const QMultiMap<QString, QString>& map):
    QMultiMap<QString, QString>(map)
{
}

Params::Params(const QHash<QString, QString>& hash)
{
    for (auto it = hash.begin(); it != hash.end(); ++it)
        insert(it.key(), it.value());
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
    const auto jsonValue =
        [](const QJsonValue& value) -> QString
    {
        if (value.type() == QJsonValue::Null)
            return QString();

        if (value.type() == QJsonValue::Bool)
            return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");

        if (value.type() == QJsonValue::Double)
            return QString::number(value.toDouble());

        // TODO: Add some format for Array and Object.
        return value.toString();
    };

    Params params;
    for (auto it = value.begin(); it != value.end(); ++it)
        params.insert(it.key(), jsonValue(it.value()));
    return params;
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
    for (auto it = begin(); it != end(); ++it)
        list.append({it.key(), it.value()});
    return list;
}

QJsonObject Params::toJson() const
{
    const auto jsonValue =
        [](const QString& value) -> QJsonValue
    {
        if (value.isEmpty())
            return QJsonValue(QJsonValue::Null);

        // TODO: Add some format for Array and Object.
        return QJsonValue(value);
    };

    QJsonObject object;
    for (auto it = begin(); it != end(); ++it)
        object.insert(it.key(), jsonValue(it.value()));
    return object;
}

std::optional<QJsonValue> Content::parse() const
{
    if (type == kFormContentType)
        return Params::fromUrlQuery(QUrlQuery(body)).toJson();

    if (type == kJsonContentType)
    {
        QJsonValue value;
        if (!QJson::deserialize(body, &value))
            return std::nullopt;

        return value;
    }

    // TODO: Other content types should go there when supported.
    return std::nullopt;
}

QString Exception::message() const
{
    return descriptor.text();
}

} // namespace nx::network::rest
