// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "byte_array.h"

#include <nx/kit/utils.h>
#include <nx/utils/log/assert.h>

namespace nx::utils {

ByteArray::ByteArray(
    size_t alignment,
    size_t capacity,
    size_t padding)
    :
    m_alignment(alignment),
    m_padding(padding)
{
    NX_ASSERT(m_alignment != 0, "Alignment could not be zero!");
    if (capacity > 0)
        reallocate(capacity);
}

ByteArray::~ByteArray()
{
    nx::kit::utils::freeAligned(m_data);
}

void ByteArray::clear()
{
    m_size = 0;
    m_ignore = 0;
    zeroPadding();
}

char* ByteArray::data()
{
    return m_data + m_ignore;
}

void ByteArray::ignore_first_bytes(size_t bytes_to_ignore)
{
    m_ignore = bytes_to_ignore;
}

int ByteArray::getAlignment() const
{
    return m_alignment;
}

size_t ByteArray::write(const char* data, size_t size)
{
    reserve(m_size + size);

    memcpy(m_data + m_size, data, size);

    m_size += size;
    zeroPadding();

    return size;
}

void ByteArray::uncheckedWrite(const char* data, size_t size)
{
    NX_ASSERT(m_size + size <= m_capacity, "Buffer MUST be preallocated!");
    memcpy(m_data + m_size, data, size); // 1s
    m_size += size;
    zeroPadding();
}

size_t ByteArray::write(quint8 value)
{
    reserve(m_size + 1);
    memcpy(m_data + m_size, &value, 1);

    m_size += 1;
    zeroPadding();

    return 1;
}

size_t ByteArray::writeAt(const char* data, size_t size, int pos)
{
    reserve(pos + size);

    memcpy(m_data + pos, data, size);

    if (size + pos > m_size)
    {
        m_size = size + pos;
        zeroPadding();
    }

    return size;
}

void ByteArray::writeFiller(quint8 filler, int size)
{
    reserve(m_size + size);
    memset(m_data + m_size, filler, size);
    m_size += size;
    zeroPadding();
}

char* ByteArray::startWriting(size_t size)
{
    reserve(m_size + size);
    return m_data + m_size;
}

void ByteArray::resize(size_t size)
{
    reserve(size);

    m_size = size;
    zeroPadding();
}

void ByteArray::reserve(size_t size)
{
    if (size <= m_capacity)
        return;

    const size_t newSize = qMax(m_capacity * 2, size);
    const bool success = reallocate(newSize);
    NX_ASSERT(success, "Could not reserve %1 bytes.", newSize);
}

void ByteArray::removeTrailingZeros(int maxBytesToRemove)
{
    while (m_size > 0 && m_data[m_size - 1] == 0 && maxBytesToRemove > 0)
    {
        --m_size;
        --maxBytesToRemove;
    }
    zeroPadding();
}

ByteArray::ByteArray(const ByteArray& other)
{
    *this = other;
}

ByteArray::ByteArray(ByteArray&& other) noexcept
{
    *this = std::move(other);
}

ByteArray& ByteArray::operator=(const ByteArray& right)
{
    if (&right == this)
        return *this;

    nx::kit::utils::freeAligned(m_data);

    m_alignment = right.m_alignment;
    m_capacity = right.m_size;
    m_size = right.m_size;
    m_padding = right.m_padding;

    m_data = allocateBuffer(m_capacity);

    memcpy(m_data, right.constData(), right.size());
    m_ignore = 0;
    zeroPadding();

    return *this;
}

ByteArray& ByteArray::operator=(ByteArray&& right) noexcept
{
    if (&right == this)
        return *this;

    nx::kit::utils::freeAligned(m_data);

    m_alignment = std::move(right.m_alignment);
    m_capacity = std::move(right.m_size);
    m_size = std::move(right.m_size);
    m_padding = std::move(right.m_padding);
    m_ignore = std::move(right.m_ignore);
    m_data = std::move(right.m_data);

    // Avoid data double-free.
    right.m_data = nullptr;

    return *this;
}

void ByteArray::zeroPadding()
{
    // If the padding bytes are not zeros, then damaged MPEG bitstreams could cause overread and
    // segfault in the optimized bitstream readers.
    if (m_data && m_padding)
        memset(m_data + m_size, 0, m_padding);
}

char* ByteArray::allocateBuffer(size_t capacity)
{
    return (char*) nx::kit::utils::mallocAligned((size_t) capacity + m_padding, m_alignment);
}

bool ByteArray::reallocate(size_t capacity)
{
    if (!(NX_ASSERT(capacity >= m_size,
            "Unable to decrease capacity. Did you forget to clear() the buffer?")))
    {
        return false;
    }

    if (capacity < m_capacity)
        return true;

    char* data = allocateBuffer(capacity);

    if (!data)
        return false;

    if (m_data && m_size)
        memcpy(data, m_data, m_size);

    if (m_data)
        nx::kit::utils::freeAligned(m_data);

    m_capacity = capacity;
    m_data = data;
    zeroPadding();

    return true;
}

} // namespace nx::utils
