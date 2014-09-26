/**********************************************************
* 24 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_UUID_H
#define NX_UUID_H

#include <boost/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QDataStream>
#include <QDebug>
#include <QMetaType>


//!This class added to minimise creation/destruction of QByteArray and QString
class QnUuid
{
public:
    QnUuid();
    QnUuid( const char* text );
    QnUuid( const QString& text );
    QnUuid( const QByteArray& text );

    const QUuid& getQUuid() const
    {
        return m_uuid;
    }

    bool isNull() const { return m_uuid.isNull(); }
    const QByteArray& toByteArray() const;
    const QByteArray& toRfc4122() const;
    const QString& toString() const;

    bool operator!=( const QnUuid& other ) const
    {
        return m_uuid != other.m_uuid;
    }

    bool operator==( const QnUuid& other ) const
    {
        return m_uuid == other.m_uuid;
    }

    bool operator<( const QnUuid& other ) const
    {
        return m_uuid < other.m_uuid;
    }

    bool operator>( const QnUuid& other ) const
    {
        return m_uuid > other.m_uuid;
    }

    static QnUuid fromRfc4122( const QByteArray& bytes );
    static QnUuid createUuid();

private:
    QUuid m_uuid;
    mutable boost::optional<QString> m_stringRepresentation;
    //!text representation
    mutable boost::optional<QByteArray> m_byteArrayRepresentation;
    //!binary representation
    mutable boost::optional<QByteArray> m_rfc4122Representation;

    friend QDataStream& operator>>(QDataStream& s, QnUuid& id);
};

Q_DECLARE_METATYPE(QnUuid);

uint qHash( const QnUuid& uuid, uint seed = 0 ) throw();
QDataStream& operator<<(QDataStream& s, const QnUuid& id);
QDebug operator<<(QDebug dbg, const QnUuid& id);
QDataStream& operator>>(QDataStream& s, QnUuid& id);

#endif  //NX_UUID_H
