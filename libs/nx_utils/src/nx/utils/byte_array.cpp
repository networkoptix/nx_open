// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "byte_array.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

#include <nx/kit/utils.h>
#include <nx/utils/log/assert.h>

namespace nx::utils {

static_assert(ByteArray::kMinAlignment == nx::kit::utils::kMinAlignment);

namespace {

/**
 * Sum of a current size and an increment, both of which may be externally controlled.
 *
 * \throws std::length_error If the sum does not fit size_t. Letting it wrap would make reserve()
 *     see a small number, keep the old buffer, and hand the caller a size the buffer cannot hold.
 */
size_t checkedSum(size_t size, size_t increment)
{
    if (size > std::numeric_limits<size_t>::max() - increment)
    {
        throw std::length_error("nx::utils::ByteArray: the requested size does not fit size_t.");
    }

    return size + increment;
}

} // namespace

ByteArray::ByteArray(size_t capacity, size_t alignment, size_t padding):
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
    zeroPadding();
}

char* ByteArray::data()
{
    return m_data;
}

size_t ByteArray::write(const char* data, size_t size)
{
    reserve(checkedSum(m_size, size));

    // memcpy() requires valid pointers even for a zero length: both m_data and data may be null
    // here, and reserve() allocates nothing when there is nothing to append.
    if (size)
        memcpy(m_data + m_size, data, size);

    m_size += size;
    zeroPadding();

    return size;
}

size_t ByteArray::writeAt(const char* data, size_t size, int pos)
{
    reserve(checkedSum((size_t) pos, size));

    if (size)
        memcpy(m_data + pos, data, size);

    if (size + pos > m_size)
    {
        m_size = size + pos;
        zeroPadding();
    }

    return size;
}

void ByteArray::writeFiller(uint8_t filler, int size)
{
    reserve(checkedSum(m_size, (size_t) size));
    if (size > 0)
        memset(m_data + m_size, filler, size);
    m_size += size;
    zeroPadding();
}

char* ByteArray::startWriting(size_t size)
{
    reserve(checkedSum(m_size, size));
    return m_data + m_size;
}

void ByteArray::finishWriting(size_t size)
{
    const size_t newSize = checkedSum(m_size, size);

    // The data has already been written through the pointer from startWriting(), so a size over
    // the capacity means the caller has overrun the buffer. Keeping m_size inside the buffer at
    // least leaves zeroPadding() below and the next reallocate() in bounds.
    NX_ASSERT(newSize <= m_capacity,
        "Reported %1 bytes written over the capacity %2.",
        newSize,
        m_capacity);
    m_size = std::min(newSize, m_capacity);

    zeroPadding();
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

    // Growing by doubling, unless doubling overflows - in that case the exact requested size is
    // the best that can still be represented.
    const size_t newSize = (m_capacity > std::numeric_limits<size_t>::max() / 2)
        ? size
        : std::max(m_capacity * 2, size);

    reallocate(newSize);
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

    // Allocate before releasing the old buffer: if the allocation throws, this array has to be
    // left untouched. Freeing first would leave m_data dangling and free it again in the
    // destructor.
    char* data = allocateBuffer(right.m_size, right.m_alignment, right.m_padding);
    if (right.m_size)
        memcpy(data, right.constData(), right.size());

    nx::kit::utils::freeAligned(m_data);

    m_alignment = right.m_alignment;
    m_capacity = right.m_size;
    m_size = right.m_size;
    m_padding = right.m_padding;
    m_data = data;
    zeroPadding();

    return *this;
}

ByteArray& ByteArray::operator=(ByteArray&& right) noexcept
{
    if (&right == this)
        return *this;

    nx::kit::utils::freeAligned(m_data);

    m_alignment = right.m_alignment;
    m_capacity = right.m_capacity;
    m_size = right.m_size;
    m_padding = right.m_padding;
    m_data = right.m_data;

    // Avoid data double-free.
    right.m_data = nullptr;
    right.m_capacity = 0;
    right.m_size = 0;
    return *this;
}

void ByteArray::zeroPadding()
{
    // If the padding bytes are not zeros, then damaged MPEG bitstreams could cause overread and
    // segfault in the optimized bitstream readers.
    if (m_data && m_padding)
        memset(m_data + m_size, 0, m_padding);
}

char* ByteArray::allocateBuffer(size_t capacity, size_t alignment, size_t padding)
{
    if (capacity > std::numeric_limits<size_t>::max() - padding)
        throw std::length_error("nx::utils::ByteArray: requested capacity does not fit size_t.");

    char* data = (char*) nx::kit::utils::mallocAligned(capacity + padding, alignment);
    if (!data)
        throw std::bad_alloc();

    return data;
}

void ByteArray::reallocate(size_t capacity)
{
    if (!(NX_ASSERT(capacity >= m_size, "Unable to decrease capacity.")))
        return;

    if (capacity < m_capacity)
        return;

    char* data = allocateBuffer(capacity, m_alignment, m_padding);

    if (m_data && m_size)
        memcpy(data, m_data, m_size);

    if (m_data)
        nx::kit::utils::freeAligned(m_data);

    m_capacity = capacity;
    m_data = data;
    zeroPadding();
}

} // namespace nx::utils
