// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QUrlQuery>

#include <nx/network/http/http_types.h>
#include <nx/string.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/serialization/format.h>

namespace nx::network::rest {

class NX_VMS_COMMON_API Params
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
    void clear() { m_values.clear(); }

    auto keyValueRange() const
    {
        return nx::utils::keyValueRange(m_values);
    }

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

    QString toString() const { return nx::containerString(toList()); }

private:
    QMultiMap<QString, QString> m_values;
};

struct NX_VMS_COMMON_API Content
{
    http::header::ContentType type;
    nx::String body;

    std::optional<QJsonValue> parse() const;
    QJsonValue parseOrThrow() const;
};

} // namespace nx::network::rest
