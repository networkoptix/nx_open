#include "byte_array.h"
#include "warnings.h"
#include "plugins/plugin_tools.h"

/**
 * Same as FF_INPUT_BUFFER_PADDING_SIZE.
 */
#define QN_BYTE_ARRAY_PADDING 16

//#define CALC_QnByteArray_ALLOCATION
#ifdef CALC_QnByteArray_ALLOCATION
QAtomicInt QnByteArray_bytesAllocated;
#endif

#if 1
#define _qMallocAligned qMallocAligned
#define _qFreeAligned qFreeAligned
#else
#define _qMallocAligned nxpt::mallocAligned
#define _qFreeAligned nxpt::freeAligned
#endif

QnByteArray::QnByteArray(unsigned int alignment, unsigned int capacity):
    m_alignment(alignment),
    m_capacity(0),
    m_size(0),
    m_data(NULL),
    m_ignore(0),
    m_ownBuffer( true )
{
    if (capacity > 0)
        reallocate(capacity);
}

QnByteArray::QnByteArray( char* buf, unsigned int dataSize )
:
    m_alignment( 1 ),
    m_capacity( dataSize ),
    m_size( dataSize ),
    m_data( buf ),
    m_ignore( 0 ),
    m_ownBuffer( false )
{
}

QnByteArray::~QnByteArray()
{
    if( m_ownBuffer )
    {
        _qFreeAligned(m_data);
#ifdef CALC_QnByteArray_ALLOCATION
        QnByteArray_bytesAllocated.fetchAndAddOrdered( -(int)(m_capacity + QN_BYTE_ARRAY_PADDING) );
#endif
    }
}

void QnByteArray::clear()
{
    m_size = 0;
    m_ignore = 0;
}

char *QnByteArray::data()
{
    if( !m_ownBuffer )
        reallocate( m_capacity );

    return m_data + m_ignore;
}

void QnByteArray::ignore_first_bytes(int bytes_to_ignore)
{
    m_ignore = bytes_to_ignore;
}

int QnByteArray::getAlignment() const
{
    return m_alignment;
}

unsigned int QnByteArray::write( const char *data, unsigned int size )
{
    if( !m_ownBuffer )
        reallocate( m_capacity );

    reserve(m_size + size);

    memcpy(m_data + m_size, data, size);

    m_size += size;

    return size;
}

unsigned int QnByteArray::write(quint8 value)
{
    if( !m_ownBuffer )
        reallocate( m_capacity );

    reserve(m_size + 1);

    memcpy(m_data + m_size, &value, 1);

    m_size += 1;

    return 1;
}

unsigned int QnByteArray::writeAt(const char *data, unsigned int size, int pos)
{
    if( !m_ownBuffer )
        reallocate( m_capacity );

    reserve(pos + size);

    memcpy(m_data + pos, data, size);

    if (size + pos > m_size)
        m_size = size + pos;

    return size;
}

void QnByteArray::writeFiller(quint8 filler, int size)
{
    if( !m_ownBuffer )
        reallocate( m_capacity );

    reserve(m_size + size);

    memset(m_data + m_size, filler, size);
    m_size += size;
}

char *QnByteArray::startWriting(unsigned int size)
{
    if( !m_ownBuffer )
        reallocate( m_capacity );

    reserve(m_size + size);

    return m_data + m_size;
}

void QnByteArray::resize(unsigned int size)
{
    reserve(size);

    m_size = size;
}

void QnByteArray::reserve(unsigned int size)
{
    if(size <= m_capacity)
        return;

    if(!reallocate(qMax(m_capacity * 2, size)))
        qnWarning("Could not reserve '%1' bytes.", size);
}

void QnByteArray::removeTrailingZeros()
{
    while (m_size > 0 && m_data[m_size - 1] == 0)
        --m_size;
}

QnByteArray& QnByteArray::operator=( const QnByteArray& right )
{
    if( &right == this )
        return *this;

    if( m_ownBuffer )
    {
#ifdef CALC_QnByteArray_ALLOCATION
        QnByteArray_bytesAllocated.fetchAndAddOrdered( -(int)(m_capacity + QN_BYTE_ARRAY_PADDING) );
#endif
        _qFreeAligned(m_data);
    }

    m_alignment = right.m_alignment;
    m_capacity = right.m_size;
    m_size = right.m_size;
    m_data = (char*)_qMallocAligned( m_capacity + QN_BYTE_ARRAY_PADDING, m_alignment ); 
#ifdef CALC_QnByteArray_ALLOCATION
    QnByteArray_bytesAllocated.fetchAndAddOrdered( m_capacity + QN_BYTE_ARRAY_PADDING );
#endif
    memcpy( m_data, right.constData(), right.size() );
    m_ignore = 0;
    m_ownBuffer = true;

    return *this;
}

QnByteArray& QnByteArray::operator=( QnByteArray&& right )
{
    if( &right == this )
        return *this;

    if( m_ownBuffer )
    {
        _qFreeAligned(m_data);
#ifdef CALC_QnByteArray_ALLOCATION
        QnByteArray_bytesAllocated.fetchAndAddOrdered( -(int)(m_capacity + QN_BYTE_ARRAY_PADDING) );
#endif
    }

    m_alignment = right.m_alignment;
    m_capacity = right.m_capacity;
    m_size = right.m_size;
    m_data = right.m_data;
    m_ignore = right.m_ignore;
    m_ownBuffer = right.m_ownBuffer;

    right.m_alignment = 1;
    right.m_capacity = 0;
    right.m_size = 0;
    right.m_data = nullptr;
    right.m_ignore = 0;
    right.m_ownBuffer = true;

    return *this;
}

bool QnByteArray::reallocate(unsigned int capacity)
{
    Q_ASSERT(capacity > 0);

    if (capacity < m_size)
    {
        qWarning("QnByteArray::reallocate(): Unable to decrease capacity. "
                 "Did you forget to clear() the buffer?");
        return false;
    }

    if (capacity < m_capacity)
        return true;

    char *data = (char *) _qMallocAligned(capacity + QN_BYTE_ARRAY_PADDING, m_alignment);
    if(!data)
        return false;
#ifdef CALC_QnByteArray_ALLOCATION
    QnByteArray_bytesAllocated.fetchAndAddOrdered( capacity + QN_BYTE_ARRAY_PADDING );
#endif

    memcpy(data, m_data, m_size);
    if( m_ownBuffer )
    {
        _qFreeAligned(m_data);
#ifdef CALC_QnByteArray_ALLOCATION
        QnByteArray_bytesAllocated.fetchAndAddOrdered( -(int)(m_capacity + QN_BYTE_ARRAY_PADDING) );
#endif
    }
    m_capacity = capacity;
    m_data = data;
    m_ownBuffer = true;

    return true;
}
