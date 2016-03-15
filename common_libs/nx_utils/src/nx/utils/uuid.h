/**********************************************************
* 24 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_UUID_H
#define NX_UUID_H

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


//!This class added to minimize creation/destruction of QByteArray and QString
class NX_UTILS_API QnUuid
{
public:
    static const size_t RFC4122_SIZE = 16;

    QnUuid();
    QnUuid(const QnUuid& other) = default;
    QnUuid(QnUuid&& other) = default;

    explicit QnUuid( const char* text );
    explicit QnUuid( const QString& text );
    explicit QnUuid( const QByteArray& text );
    explicit QnUuid( const std::string& text );
    QnUuid( const QUuid &uuid );

    const QUuid& getQUuid() const;

    bool isNull() const;
    const QByteArray& toByteArray() const;
    const QByteArray& toRfc4122() const;
    const QString& toString() const;
    QString toSimpleString() const;
    std::string toStdString() const;
    QUuid toQUuid() const;

    bool operator!=( const QnUuid& other ) const;
    bool operator==( const QnUuid& other ) const;
    bool operator<( const QnUuid& other ) const;
    bool operator>( const QnUuid& other ) const;

    QnUuid& operator=(const QnUuid& other) = default;
    QnUuid& operator=(QnUuid&& other) = default;

    static QnUuid fromRfc4122( const QByteArray& bytes );
    static QnUuid fromHardwareId( const QString& hwid );
    static QnUuid createUuid();

    /** Construct QnUuid from pool of id's. Pool is determined by base id and individual offset. */
    static QnUuid createUuidFromPool(const QUuid &baseId, uint offset);

    /*! Construct QnUuid from string representation.
     * If the string is not a valid UUID null QnUuid will be returned.
     */
    static QnUuid fromStringSafe( const QString& uuid );
    static QnUuid fromStringSafe( const QByteArray& uuid );

private:
    QUuid m_uuid;
    mutable boost::optional<QString> m_stringRepresentation;
    //!text representation
    mutable boost::optional<QByteArray> m_byteArrayRepresentation;
    //!binary representation
    mutable boost::optional<QByteArray> m_rfc4122Representation;

    friend NX_UTILS_API QDataStream& operator>>(QDataStream& s, QnUuid& id);
};

Q_DECLARE_METATYPE(QnUuid);

NX_UTILS_API uint qHash( const QnUuid& uuid, uint seed = 0 ) throw();
NX_UTILS_API QDataStream& operator<<(QDataStream& s, const QnUuid& id);
NX_UTILS_API QDebug operator<<(QDebug dbg, const QnUuid& id);
NX_UTILS_API QDataStream& operator>>(QDataStream& s, QnUuid& id);

#endif  //NX_UUID_H
