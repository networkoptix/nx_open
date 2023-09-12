// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <string_view>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUuid>

#include <nx/reflect/instrument.h>

class NX_UTILS_API QnUuid
{
    Q_GADGET

public:
    static const size_t RFC4122_SIZE = 16;

    QnUuid();
    QnUuid(const QnUuid&) = default;
    QnUuid(QnUuid&&) = default;

    explicit QnUuid(const char* text);
    explicit QnUuid(const QString& text);
    explicit QnUuid(const QByteArray& text);
    explicit QnUuid(const std::string& text);
    QnUuid(const QUuid& uuid);

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

    bool operator<( const QnUuid& other ) const;
    bool operator>( const QnUuid& other ) const;

    constexpr bool operator!=(const QnUuid& other) const
    {
        return m_uuid != other.m_uuid;
    }

    constexpr bool operator==(const QnUuid& other) const
    {
        return m_uuid == other.m_uuid;
    }

    QnUuid& operator=(const QnUuid&) = default;
    QnUuid& operator=(QnUuid&&) = default;

    static QnUuid fromRfc4122( const QByteArray& bytes );
    static QnUuid fromHardwareId( const QString& hwid );
    static QnUuid createUuid();

    /** Construct QnUuid from pool of id's. Pool is determined by base id and individual offset. */
    static QnUuid createUuidFromPool(const QUuid &baseId, uint offset);

    /**
     * Construct QnUuid from string representation.
     * If the string is not a valid UUID null QnUuid will be returned.
     */
    static QnUuid fromStringSafe(const QString& uuid);
    static QnUuid fromStringSafe(const QByteArray& uuid);
    static QnUuid fromStringSafe(const char* uuid);
    static QnUuid fromStringSafe(const std::string_view& uuid);

    /**
     * Create fixed QnUuid from any data. As a value of uuid the MD5 hash is taken so created uuids
     * will be equal for equal strings given. Also there is a collision possibility.
     */
    static QnUuid fromArbitraryData(const QByteArray& data);
    static QnUuid fromArbitraryData(const QString& data);
    static QnUuid fromArbitraryData(std::string_view data);

    template<size_t N>
    static QnUuid fromArbitraryData(const char (&data)[N])
    {
        return fromArbitraryData(std::string_view(data, N));
    }

    static bool isUuidString(const QByteArray& data);
    static bool isUuidString(const QString& data);
    static bool isUuidString(const std::string& data);

    static QnUuid fromString(const std::string_view& str);

private:
    QUuid m_uuid;

    friend NX_UTILS_API QDataStream& operator>>(QDataStream& s, QnUuid& id);
};

NX_REFLECTION_TAG_TYPE(QnUuid, useStringConversionForSerialization)

namespace std {

template<>
struct hash<QnUuid>
{
    size_t operator()(const QnUuid& id) const
    {
        return qHash(id.getQUuid());
    }
};

} // namespace std

namespace nx::utils {

NX_UTILS_API QString changedGuidByteOrder(const QString& guid);

} // namespace nx::utils

using QnUuidSet = QSet<QnUuid>;
using QnUuidList = QList<QnUuid>;

NX_UTILS_API size_t qHash(const QnUuid& uuid, size_t seed = 0 ) noexcept;
NX_UTILS_API QDataStream& operator<<(QDataStream& s, const QnUuid& id);
NX_UTILS_API QDebug operator<<(QDebug dbg, const QnUuid& id);
NX_UTILS_API QDataStream& operator>>(QDataStream& s, QnUuid& id);
