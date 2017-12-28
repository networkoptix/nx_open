#pragma once

#include <string>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QDataStream>
#include <QDebug>
#include <QMetaType>

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
    Q_INVOKABLE const QString toString() const;
    /** @return GUID without braces */
    Q_INVOKABLE QString toSimpleString() const;
    QByteArray toSimpleByteArray() const;
    std::string toStdString() const;
    QUuid toQUuid() const;

    bool operator!=( const QnUuid& other ) const;
    bool operator==( const QnUuid& other ) const;
    bool operator<( const QnUuid& other ) const;
    bool operator>( const QnUuid& other ) const;

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
    static QnUuid fromStringSafe(const std::string& uuid);

private:
    QUuid m_uuid;

    friend NX_UTILS_API QDataStream& operator>>(QDataStream& s, QnUuid& id);
};

namespace std
{
    template<>
    struct hash<QnUuid>
    {
        size_t operator()(const QnUuid& id) const
        {
            return qHash(id.toQUuid());
        }
    };
}

Q_DECLARE_METATYPE(QnUuid);

NX_UTILS_API uint qHash( const QnUuid& uuid, uint seed = 0 ) throw();
NX_UTILS_API QDataStream& operator<<(QDataStream& s, const QnUuid& id);
NX_UTILS_API QDebug operator<<(QDebug dbg, const QnUuid& id);
NX_UTILS_API QDataStream& operator>>(QDataStream& s, QnUuid& id);
NX_UTILS_API QString toString(const QnUuid& id);
