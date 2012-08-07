#include "bytearray.h"
#include "warnings.h"

/**
 * Same as FF_INPUT_BUFFER_PADDING_SIZE.
 */
#define QN_BYTE_ARRAY_PADDING 16

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
    qFreeAligned(m_data);
}

void QnByteArray::clear()
{
    m_size = 0;
    m_ignore = 0;
}

const char *QnByteArray::constData() const
{
    return m_data + m_ignore;
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

unsigned int QnByteArray::size() const
{
    return m_size - m_ignore;
}

unsigned int QnByteArray::capacity() const
{
    return m_capacity;
}

unsigned int QnByteArray::write( const char *data, unsigned int size )
{
    if( !m_ownBuffer )
        reallocate( m_capacity );

    reserve(m_size + size);

    qMemCopy(m_data + m_size, data, size);

    m_size += size;

    return size;
}

unsigned int QnByteArray::writeAt(const char *data, unsigned int size, int pos)
{
    if( !m_ownBuffer )
        reallocate( m_capacity );

    reserve(pos + size);

    qMemCopy(m_data + pos, data, size);

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

void QnByteArray::finishWriting(unsigned int size)
{
    m_size += size;
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

    char *data = (char *) qMallocAligned(capacity + QN_BYTE_ARRAY_PADDING, m_alignment);
    if(!data)
        return false;

    qMemCopy(data, m_data, m_size);
    if( m_ownBuffer )
        qFreeAligned(m_data);
    m_capacity = capacity;
    m_data = data;
    m_ownBuffer = true;

    return true;
}
