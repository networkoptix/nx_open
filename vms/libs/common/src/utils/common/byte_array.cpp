#include "byte_array.h"

#include <nx/utils/log/assert.h>
#include <nx/kit/utils.h>

QnByteArray::QnByteArray(
    size_t alignment,
    size_t capacity,
    size_t padding)
    :
    m_alignment(alignment),
    m_padding(padding)
{
    if (capacity > 0)
        reallocate(capacity);
}

QnByteArray::~QnByteArray()
{
    nx::kit::utils::freeAligned(m_data);
}

void QnByteArray::clear()
{
    m_size = 0;
    m_ignore = 0;
}

char* QnByteArray::data()
{
    return m_data + m_ignore;
}

void QnByteArray::ignore_first_bytes(size_t bytes_to_ignore)
{
    m_ignore = bytes_to_ignore;
}

int QnByteArray::getAlignment() const
{
    return m_alignment;
}

size_t QnByteArray::write(const char* data, size_t size)
{
    reserve(m_size + size);

    memcpy(m_data + m_size, data, size);

    m_size += size;

    return size;
}

void QnByteArray::uncheckedWrite(const char* data, size_t size)
{
    NX_ASSERT(m_size + size <= m_capacity, "Buffer MUST be preallocated!");
    memcpy(m_data + m_size, data, size); // 1s
    m_size += size;
}

size_t QnByteArray::write(quint8 value)
{
    reserve(m_size + 1);
    memcpy(m_data + m_size, &value, 1);

    m_size += 1;

    return 1;
}

size_t QnByteArray::writeAt(const char* data, size_t size, int pos)
{
    reserve(pos + size);

    memcpy(m_data + pos, data, size);

    if (size + pos > m_size)
        m_size = size + pos;

    return size;
}

void QnByteArray::writeFiller(quint8 filler, int size)
{
    reserve(m_size + size);
    memset(m_data + m_size, filler, size);
    m_size += size;
}

char* QnByteArray::startWriting(size_t size)
{
    reserve(m_size + size);
    return m_data + m_size;
}

void QnByteArray::resize(size_t size)
{
    reserve(size);

    m_size = size;
}

void QnByteArray::reserve(size_t size)
{
    if (size <= m_capacity)
        return;

    const size_t newSize = qMax(m_capacity * 2, size);
    const bool success = reallocate(newSize);
    NX_ASSERT(success, "Could not reserve %1 bytes.", newSize);
}

void QnByteArray::removeTrailingZeros()
{
    while (m_size > 0 && m_data[m_size - 1] == 0)
        --m_size;
}

QnByteArray& QnByteArray::operator=(const QnByteArray& right)
{
    if (&right == this)
        return *this;

    nx::kit::utils::freeAligned(m_data);

    m_alignment = right.m_alignment;
    m_capacity = right.m_size;
    m_size = right.m_size;
    m_padding = right.m_padding;
    m_data = (char*) nx::kit::utils::mallocAligned(
        m_capacity + m_padding,
        m_alignment);

    memcpy(m_data, right.constData(), right.size());
    m_ignore = 0;

    return *this;
}

bool QnByteArray::reallocate(size_t capacity)
{
    if (!(NX_ASSERT(capacity >= m_size,
            "Unable to decrease capacity. Did you forget to clear() the buffer?")))
    {
        return false;
    }

    if (capacity < m_capacity)
        return true;

    char* data = (char*) nx::kit::utils::mallocAligned(
        (size_t) capacity + m_padding, m_alignment);

    if (!data)
        return false;

    if (m_data && m_size)
        memcpy(data, m_data, m_size);

    // If the first 23 bits of the additional bytes are not 0, then damaged MPEG bitstreams could
    // cause overread and segfault.
    memset(data + capacity, 0, m_padding);

    if (m_data)
        nx::kit::utils::freeAligned(m_data);

    m_capacity = capacity;
    m_data = data;

    return true;
}
