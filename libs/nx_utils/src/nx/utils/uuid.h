// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <string_view>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUuid>

#include <nx/reflect/instrument.h>

namespace nx {

class NX_UTILS_API Uuid
{
    Q_GADGET

public:
    static const size_t RFC4122_SIZE = 16;

    Uuid() = default;
    Uuid(const Uuid&) = default;
    Uuid(Uuid&&) = default;

    explicit Uuid(const char* text);
    explicit Uuid(const QString& text);
    explicit Uuid(const QByteArray& text);
    explicit Uuid(const std::string& text);
    Uuid(const QUuid& uuid);

    const QUuid& getQUuid() const;

    Q_INVOKABLE bool isNull() const;
    const QByteArray toByteArray() const;
    const QByteArray toRfc4122() const;

    /** @return GUID in braces. */
    Q_INVOKABLE const QString toString() const;

    /** @return GUID without braces */
    Q_INVOKABLE QString toSimpleString() const;

    QByteArray toSimpleByteArray() const;

    std::string toStdString() const;
    /** @return GUID without braces. */
    std::string toSimpleStdString() const;

    QUuid toQUuid() const;

    bool operator<(const Uuid& other) const;
    bool operator>(const Uuid& other) const;

    constexpr bool operator!=(const Uuid& other) const
    {
        return m_uuid != other.m_uuid;
    }

    constexpr bool operator==(const Uuid& other) const
    {
        return m_uuid == other.m_uuid;
    }

    Uuid& operator=(const Uuid&) = default;
    Uuid& operator=(Uuid&&) = default;

    static Uuid fromRfc4122(const QByteArray& bytes);
    static Uuid fromHardwareId(const QString& hwid);
    static Uuid createUuid();

    /** Construct Uuid from pool of id's. Pool is determined by base id and individual offset. */
    static Uuid createUuidFromPool(const QUuid &baseId, uint offset);

    /**
     * Construct Uuid from string representation.
     * If the string is not a valid UUID null Uuid will be returned.
     */
    static Uuid fromStringSafe(const QString& uuid);
    static Uuid fromStringSafe(const QByteArray& uuid);
    static Uuid fromStringSafe(const char* uuid);
    static Uuid fromStringSafe(const std::string_view& uuid);

    /**
     * Create fixed Uuid from any data. As a value of uuid the MD5 hash is taken so created uuids
     * will be equal for equal strings given. Also there is a collision possibility.
     */
    static Uuid fromArbitraryData(const QByteArray& data);
    static Uuid fromArbitraryData(const QString& data);
    static Uuid fromArbitraryData(std::string_view data);

    template<size_t N>
    static Uuid fromArbitraryData(const char (&data)[N])
    {
        return fromArbitraryData(std::string_view(data, N));
    }

    static bool isUuidString(const QByteArray& data);
    static bool isUuidString(const QString& data);
    static bool isUuidString(const std::string& data);

    static Uuid fromString(const std::string_view& str);

private:
    QUuid m_uuid;

    friend NX_UTILS_API QDataStream& operator>>(QDataStream& s, Uuid& id);
};

NX_REFLECTION_TAG_TYPE(Uuid, useStringConversionForSerialization)

NX_UTILS_API QString changedGuidByteOrder(const QString& guid);

NX_UTILS_API QDataStream& operator<<(QDataStream& s, const Uuid& id);
NX_UTILS_API QDebug operator<<(QDebug dbg, const Uuid& id);
NX_UTILS_API QDataStream& operator>>(QDataStream& s, Uuid& id);

inline size_t qHash(const Uuid& id, size_t seed = 0)
{
    return qHash(id.getQUuid(), seed);
}

} // namespace nx

namespace std {

template<>
struct hash<nx::Uuid>
{
    size_t operator()(const nx::Uuid& id, size_t seed = 0) const
    {
        return qHash(id.getQUuid(), seed);
    }
};

} // namespace std

using UuidSet = QSet<nx::Uuid>;
using UuidList = QList<nx::Uuid>;

inline size_t qHash(const nx::Uuid& id, size_t seed = 0)
{
    return qHash(id.getQUuid(), seed);
}
