/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#include "qnbytearrayref.h"

#include <cstring>


QnByteArrayConstRef::QnByteArrayConstRef()
:
    m_src( NULL ),
    m_offset( 0 ),
    m_count( 0 )
{
}

QnByteArrayConstRef::QnByteArrayConstRef(
    const QByteArray& src,
    size_type offset,
    size_type count )
:
    m_src( &src ),
    m_offset( offset ),
    m_count( count == npos ? src.size()-offset : count )
{
    Q_ASSERT( m_offset <= (size_type)src.size() );
    Q_ASSERT( m_count <= (size_type)src.size() );
}

const QnByteArrayConstRef::value_type* QnByteArrayConstRef::data() const
{
    return constData();
}

const QnByteArrayConstRef::value_type* QnByteArrayConstRef::constData() const
{
    return m_src->data() + m_offset;
}

QnByteArrayConstRef::size_type QnByteArrayConstRef::size() const
{
    return m_count;
}

QnByteArrayConstRef QnByteArrayConstRef::mid( QnByteArrayConstRef::size_type offset, QnByteArrayConstRef::size_type count ) const
{
    if( count == npos )
        count = m_count - offset;
    return QnByteArrayConstRef( *m_src, m_offset+offset, count );
}

bool QnByteArrayConstRef::isEmpty() const
{
    return m_count == 0;
}

bool QnByteArrayConstRef::startsWith( const_pointer str, size_type len ) const
{
    if( len == npos )
        len = strlen( str );
    if( m_count < len )
        return false;
    return memcmp( data(), str, len ) == 0;
}

uint QnByteArrayConstRef::toUInt() const
{
    //TODO/IMPL effective implementation
    return ((QByteArray)*this).toUInt();
}

const QnByteArrayConstRef::value_type& QnByteArrayConstRef::operator[]( size_type index ) const
{
    Q_ASSERT( index < m_count );
    return *(constData()+index);
}

QnByteArrayConstRef::operator QByteArray() const
{
    return m_src->mid( m_offset, m_count );
}



//////////////////////////////////////////////////////////
// non-member operators
//////////////////////////////////////////////////////////

bool operator==( const QnByteArrayConstRef& left, const QByteArray& right )
{
    if( left.size() != (QnByteArrayConstRef::size_type)right.size() )
        return false;
    return memcmp( left.constData(), right.constData(), left.size() ) == 0;
}

bool operator==( const QByteArray& left, const QnByteArrayConstRef& right )
{
    return right == left;
}

bool operator!=( const QnByteArrayConstRef& left, const QByteArray& right )
{
    return !(left == right);
}

bool operator!=( const QByteArray& left, const QnByteArrayConstRef& right )
{
    return !(left == right);
}

bool operator==( const QnByteArrayConstRef::const_pointer& left, const QnByteArrayConstRef& right )
{
    const size_t leftLen = strlen( left );
    if( leftLen != right.size() )
        return false;
    return memcmp( left, right.data(), leftLen ) == 0;
}

bool operator!=( const QnByteArrayConstRef::const_pointer& left, const QnByteArrayConstRef& right )
{
    return !(left == right);
}

bool operator==( const QnByteArrayConstRef& left, const QnByteArrayConstRef::const_pointer& right )
{
    return right == left;
}

bool operator!=( const QnByteArrayConstRef& left, const QnByteArrayConstRef::const_pointer& right )
{
    return right != left;
}
