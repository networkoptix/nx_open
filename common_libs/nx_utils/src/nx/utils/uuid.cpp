/**********************************************************
* 24 sep 2014
* a.kolesnikov
***********************************************************/

#include "uuid.h"

#include <type_traits>
#include <nx/utils/log/assert.h>

QnUuid::QnUuid()
{
}

QnUuid::QnUuid( const char* text )
:
    m_uuid( text ? QByteArray::fromRawData( text, static_cast<int>(strlen(text)) ) : QByteArray() )
{
}

QnUuid::QnUuid( const QString& text )
:
    m_uuid( text )
{
    if( !text.isEmpty() )
    {
        NX_ASSERT(
            text.size() == 36 ||    // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
            text.size() == 38 );    //{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    }
}

QnUuid::QnUuid( const QByteArray& text )
:
    m_uuid( text )
{
    if( !text.isEmpty() )
    {
        NX_ASSERT(
            text.size() == 36 ||    // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
            text.size() == 38 );    //{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    }
}

QnUuid::QnUuid( const std::string& text )
:
    m_uuid( QByteArray(text.c_str()) )
{
    if( !text.empty() )
    {
        NX_ASSERT(
            text.size() == 36 ||    // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
            text.size() == 38 );    //{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    }
}

QnUuid::QnUuid(const QUuid &uuid)
:
    m_uuid(uuid)
{
}

const QUuid& QnUuid::getQUuid() const
{
    return m_uuid;
}

bool QnUuid::isNull() const
{
    return m_uuid.isNull();
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

QString QnUuid::toSimpleString() const
{
    const auto& s = toString();
    return s.mid( 1, s.length() - 2 );
}

std::string QnUuid::toStdString() const
{
    const auto& byteArray = toByteArray();
    return std::string( byteArray.constData(), byteArray.size() );
}

QUuid QnUuid::toQUuid() const
{
    return m_uuid;
}

bool QnUuid::operator!=(const QnUuid& other) const
{
    return m_uuid != other.m_uuid;
}

bool QnUuid::operator==(const QnUuid& other) const
{
    return m_uuid == other.m_uuid;
}

bool QnUuid::operator<(const QnUuid& other) const
{
    return m_uuid < other.m_uuid;
}

bool QnUuid::operator>(const QnUuid& other) const
{
    return m_uuid > other.m_uuid;
}

QnUuid QnUuid::fromRfc4122(const QByteArray& bytes)
{
    QnUuid _uuid;
    _uuid.m_uuid = QUuid::fromRfc4122( bytes );
    return _uuid;
}

QnUuid QnUuid::fromHardwareId( const QString& hwid )
{
    if (hwid.length() != 34)
        return QnUuid();

    return QnUuid(QString::fromLatin1("%1-%2-%3-%4-%5")
        .arg(hwid.mid(2, 8)).arg(hwid.mid(10, 4)).arg(hwid.mid(14, 4))
        .arg(hwid.mid(18, 4)).arg(hwid.mid(22, 12)));
}

QnUuid QnUuid::createUuid()
{
    QnUuid _uuid;
    _uuid.m_uuid = QUuid::createUuid();
    return _uuid;
}

QnUuid QnUuid::fromStringSafe(const QString &uuid)
{
    return QnUuid(QUuid(uuid));
}

QnUuid QnUuid::fromStringSafe(const QByteArray& uuid)
{
    return QnUuid(QUuid(uuid));
}

QnUuid QnUuid::createUuidFromPool(const QUuid &baseId, uint offset) {
    static_assert(sizeof(uint) <= sizeof(decltype(QUuid::data1)), "Offset type must be not greater than storage field size.");
    QUuid result = baseId;
    result.data1 += offset;
    return QnUuid(result);
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
