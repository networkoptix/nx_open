#pragma once

#include <map>

#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions.h>

namespace nx::utils {

template<typename Key, typename Value>
QJsonObject mapToJsonObject(const std::map<Key, Value>& map)
{
    QJsonObject result;

    for (const auto& [key, value]: map)
    {
        QJsonValue jsonValue;
        QJson::serialize(value, &jsonValue);
        result.insert(toString(key), jsonValue);
    }

    return result;
}

template <typename Map>
Map mapFromJsonObject(const QJsonObject& jsonObject)
{
    Map result;
    for (auto it = jsonObject.begin(); it != jsonObject.end(); ++it)
    {
        typename Map::key_type key;
        if (!QnLexical::deserialize(it.key(), &key))
            continue;

        typename Map::mapped_type value;
        if (!QJson::deserialize(it.value(), &value))
            continue;

        result.emplace(std::move(key), std::move(value));
    }

    return result;
}

inline bool boolFromString(const QString& serializedValue)
{
    return serializedValue.toLower() == "true" || serializedValue.toInt() == 1;
}

} // namespace nx::utils
