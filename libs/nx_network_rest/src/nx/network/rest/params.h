// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QUrlQuery>

#include <nx/network/http/http_types.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/merge.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/std/algorithm.h>

#include "exception.h"

namespace nx::network::rest {

class NX_NETWORK_REST_API Params
{
public:
    Params() = default;
    Params(const QMultiMap<QString, QString>& map);
    Params(const QHash<QString, QString>& hash);
    Params(std::initializer_list<std::pair<QString, QString>> list);

    Params& unite(const Params& other)
    {
        m_values.unite(other.m_values);
        return *this;
    }

    template<typename T>
    void insert(const QString& key, const T& value) { m_values.insert(key, nx::toString(value)); }

    QString operator[](const QString& key) const { return m_values.value(key); }
    QString value(const QString& key, const QString& defaultValue = QString()) const
    {
        return m_values.value(key, defaultValue);
    }
    std::optional<QString> findValue(const QString& key) const
    {
        auto it = m_values.find(key);
        return it == m_values.cend() ? std::nullopt : std::optional<QString>(*it);
    }
    QList<QString> values(const QString& key) const { return m_values.values(key); }
    QString take(const QString& key) { return m_values.take(key); }
    int remove(const QString& key) { return m_values.remove(key); }

    void replace(const QString& key, const QString& value) { m_values.replace(key, value); }
    void rename(const QString& oldName, const QString& newName);
    void clear() { m_values.clear(); }

    auto keyValueRange() const
    {
        return nx::utils::keyValueRange(m_values);
    }

    bool hasNonRefParameter() const;
    bool contains(const QString& key) const { return m_values.contains(key); }
    bool isEmpty() const { return m_values.empty(); }
    bool empty() const { return m_values.empty(); }
    bool operator==(const Params& other) const { return m_values == other.m_values; }

    static Params fromUrlQuery(const QUrlQuery& query);
    static Params fromList(const QList<QPair<QString, QString>>& list);
    static Params fromJson(const QJsonObject& value);
    static QString valueToString(QJsonValue value);

    QUrlQuery toUrlQuery() const;
    QList<QPair<QString, QString>> toList() const;
    QMultiMap<QString, QString> toMap() const;
    QJsonObject toJson(bool excludeCommon = false) const;

    using Fields = nx::reflect::DeserializationResult::Fields;

    template<typename T>
    void uniteOrThrow(T* data, Fields* fields) const;

    QString toString() const { return nx::containerString(toList()); }

private:
    static void addField(
        std::string name, nx::reflect::DeserializationResult result, Fields* fields)
    {
        nx::reflect::DeserializationResult::Field f;
        f.name = std::move(name);
        f.fields = std::move(result.fields);
        fields->push_back(std::move(f));
    }

    template<typename T>
    static nx::reflect::DeserializationResult deserializeValue(
        const QString& name, const QString& value, T* data)
    {
        if constexpr (std::is_same_v<QString, T>)
        {
            *data = value;
            return {};
        }

        std::string valueStd{value.toStdString()};
        if constexpr (std::is_same_v<std::string, T>)
        {
            *data = std::move(valueStd);
            return {};
        }

        if constexpr (std::is_same_v<nx::Uuid, T>)
        {
            *data = T::fromString(valueStd);
            return {};
        }

        auto r1 = nx::reflect::json::deserialize(
            valueStd, data, nx::reflect::json::DeserializationFlag::fields);
        if (r1)
            return r1;

        auto r2 = nx::reflect::json::deserialize(
            '"' + valueStd + '"', data, nx::reflect::json::DeserializationFlag::fields);
        if (!r2)
            throw Exception::invalidParameter(name, r1.toString());
        return r2;
    }

    template<typename Field, typename T>
    void unite(const Field& field, T* data, Fields* fields) const;

private:
    QMultiMap<QString, QString> m_values;
};

struct NX_NETWORK_REST_API Content
{
    nx::network::http::header::ContentType type;
    QByteArray body;

    std::optional<QJsonValue> parse() const;
    QJsonValue parseOrThrow() const;
};

template<typename T>
void Params::uniteOrThrow(T* data, Fields* fields) const
{
    using namespace nx::reflect;

    if constexpr ((IsAssociativeContainerV<T> && !IsSetContainerV<T>)
        || (IsUnorderedAssociativeContainerV<T> && !IsUnorderedSetContainerV<T>) )
    {
        for (auto it = m_values.begin(); it != m_values.end(); ++it)
        {
            typename T::mapped_type updated;
            auto result = deserializeValue(it.key(), it.value(), &updated);
            if (!result)
                throw Exception::invalidParameter(it.key(), result.toString());

            std::string name{it.key().toStdString()};
            auto key = nx::reflect::fromString<typename T::key_type>(name);
            auto existing = data->find(key);
            if (existing == data->end())
            {
                addField(std::move(name), std::move(result), fields);
                data->emplace(std::move(key), std::move(updated));
                return;
            }

            auto next =
                nx::utils::find_if(*fields, [&name](const auto& f) { return f.name == name; });
            if (NX_ASSERT(next))
                merge(&existing->second, &updated, std::move(result.fields), &next->fields);
        }
    }
    else if constexpr (IsInstrumentedV<T>)
    {
        nxReflectVisitAllFields(
            data, [this, data, fields](auto&&... f) { (this->unite(f, data, fields), ...); });
    }
}

template<typename Field, typename T>
void Params::unite(const Field& field, T* data, Fields* fields) const
{
    std::string name{field.name()};
    auto nameQ = QString::fromStdString(name);
    auto it = m_values.find(nameQ);
    if (it == m_values.end())
        return;

    typename Field::Type updated;
    auto result = deserializeValue(nameQ, it.value(), &updated);
    auto next = nx::utils::find_if(*fields, [&name](const auto& f) { return f.name == name; });
    if (!next)
    {
        addField(std::move(name), std::move(result), fields);
        field.set(data, std::move(updated));
        return;
    }

    nx::reflect::merge(&field.ref(data), &updated, std::move(result.fields), &next->fields);
}

} // namespace nx::network::rest
