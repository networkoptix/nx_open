/**********************************************************
* 24 sep 2014
* a.kolesnikov
***********************************************************/

#include "uuid.h"


QnUuid::QnUuid()
{
}

QnUuid::QnUuid( const char* text )
:
    m_uuid( text ? QByteArray::fromRawData( text, strlen(text) ) : QByteArray() )
{
}

QnUuid::QnUuid( const QString& text )
:
    m_uuid( text )
{
    if( !text.isEmpty() )
    {
        m_stringRepresentation = text;
        assert(
            text.size() == 36 ||    //{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
            text.size() == 38 );    //xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    }
}

QnUuid::QnUuid( const QByteArray& text )
:
    m_uuid( text )
{
    if( !text.isEmpty() )
    {
        m_byteArrayRepresentation = text;
        assert(
            text.size() == 36 ||    //{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
            text.size() == 38 );    //xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    }
}

QnUuid::QnUuid(const QUuid &uuid)
:
    m_uuid(uuid)
{
}

const QByteArray& QnUuid::toByteArray() const
{
    if( !m_byteArrayRepresentation )
        m_byteArrayRepresentation = m_uuid.toByteArray();
    return m_byteArrayRepresentation.get();
}

const QByteArray& QnUuid::toRfc4122() const
{
    if( !m_rfc4122Representation )
        m_rfc4122Representation = m_uuid.toRfc4122();
    return m_rfc4122Representation.get();
}

const QString& QnUuid::toString() const
{
    if( !m_stringRepresentation )
        m_stringRepresentation = m_uuid.toString();
    return m_stringRepresentation.get();
}

QnUuid QnUuid::fromRfc4122( const QByteArray& bytes )
{
    QnUuid _uuid;
    _uuid.m_uuid = QUuid::fromRfc4122( bytes );
    if( !bytes.isEmpty() )
        _uuid.m_rfc4122Representation = bytes;
    return _uuid;
}

QnUuid QnUuid::createUuid()
{
    QnUuid _uuid;
    _uuid.m_uuid = QUuid::createUuid();
    return _uuid;
}

uint qHash( const QnUuid& uuid, uint seed ) throw()
{
    return qHash( uuid.getQUuid(), seed );
}

QDataStream& operator<<(QDataStream& s, const QnUuid& id)
{
    return s << id.getQUuid();
}

QDebug operator<<(QDebug dbg, const QnUuid& id)
{
    return dbg << id.getQUuid();
}

QDataStream& operator>>(QDataStream& s, QnUuid& id)
{
    return s >> id.m_uuid;
}
