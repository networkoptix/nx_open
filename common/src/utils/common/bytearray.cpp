#include "bytearray.h"

#include "log.h"

// same as FF_INPUT_BUFFER_PADDING_SIZE
#define CLBYTEARRAY_PADDING_SIZE 16

CLByteArray::CLByteArray(unsigned int alignment, unsigned int capacity):
    m_alignment(alignment),
    m_capacity(0),
    m_size(0),
    m_data(0),
    m_ignore(0)
{
    if (capacity > 0)
        reallocate(capacity);
}

CLByteArray::~CLByteArray()
{
    qFreeAligned(m_data);
}

void CLByteArray::clear()
{
    m_size = 0;
    m_ignore = 0;
}

const char *CLByteArray::constData() const
{
    return m_data + m_ignore;
}

char *CLByteArray::data()
{
    return m_data + m_ignore;
}

// ### consider deprecating ignore_first_bytes() as it could break data alignment
void CLByteArray::ignore_first_bytes(int bytes_to_ignore)
{
    m_ignore = bytes_to_ignore;
}

unsigned int CLByteArray::size() const
{
    return m_size - m_ignore;
}

unsigned int CLByteArray::capacity() const
{
    return m_capacity;
}

bool CLByteArray::reallocate(unsigned int new_capacity)
{
    Q_ASSERT(new_capacity > 0);

    if (new_capacity < m_size)
    {
        qWarning("CLByteArray::reallocate(): Unable to decrease capacity. "
                 "Did you forget to clean() the buffer?");
        return false;
    }

    if (new_capacity <= m_capacity)
        return true;

    //char *new_data = (char *)qReallocAligned(m_data, new_capacity + CLBYTEARRAY_PADDING_SIZE, m_capacity, m_alignment);
    char *new_data = (char *)qMallocAligned(new_capacity + CLBYTEARRAY_PADDING_SIZE, m_alignment);
    qMemCopy(new_data, m_data, m_size);
    qFreeAligned(m_data);

    if (!q_check_ptr(new_data))
        return false; // ### would probably crash anyways

    m_capacity = new_capacity;
    m_data = new_data;
    // size remains unchanged

    return true;
}

unsigned int CLByteArray::write(const char *data, unsigned int size)
{
    if (size > m_capacity - m_size) // if we do not have enough space
    {
        if (!reallocate(m_capacity * 2 + size))
        {
            cl_log.log(QLatin1String("CLByteArray::write(): Unable to increase capacity"), cl_logWARNING);
            return 0;
        }
    }

    qMemCopy(m_data + m_size, data, size);

    m_size += size;

    return size;
}

unsigned int CLByteArray::write(const char *data, unsigned int size, int abs_shift)
{
    if (size+abs_shift > m_capacity) // if we do not have enough space
    {
        if (!reallocate(qMax(m_capacity * 2 + size, size + abs_shift)))
        {
            cl_log.log(QLatin1String("CLByteArray::write(): Unable to increase capacity"), cl_logWARNING);
            return 0;
        }
    }

    qMemCopy(m_data + abs_shift, data, size);

    if (size + abs_shift > m_size)
        m_size = size + abs_shift;

    return size;
}

char *CLByteArray::prepareToWrite(unsigned int size)
{
    if (size > m_capacity - m_size) // if we do not have enough space
    {
        if (!reallocate(m_capacity * 2 + size))
        {
            cl_log.log(QLatin1String("CLByteArray::prepareToWrite(): Unable to increase capacity"), cl_logWARNING);
            return 0;
        }
    }

    return m_data + m_size;
}

void CLByteArray::done(unsigned int size)
{
    m_size += size;
}

void CLByteArray::resize(unsigned int size)
{
    m_size = size;
}

void CLByteArray::removeZerosAtTheEnd()
{
    while (m_size>0 && m_data[m_size-1] == 0)
        --m_size;
}

void CLByteArray::fill(quint8 filler, int size)
{
    // if we do not have enough space
    if (size > m_capacity - m_size && !reallocate(m_capacity * 2 + size)) 
    {
        qWarning() << Q_FUNC_INFO << "Unable to increase capacity";
        return;
    }
    memset(m_data + m_size, filler, size);
    m_size += size;
}
