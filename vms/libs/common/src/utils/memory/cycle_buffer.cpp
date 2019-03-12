#include "cycle_buffer.h"

#include <nx/utils/log/assert.h>

QnMediaCyclicBuffer::QnMediaCyclicBuffer(size_type bufferSize, int align):
    m_buffer(0),
    m_maxSize(bufferSize),
    m_size(0),
    m_offset(0),
    m_align(align)
{
    if (bufferSize > 0) {
        NX_ASSERT(bufferSize >= align);
        m_buffer = (value_type*) qMallocAligned(bufferSize, align);
        NX_ASSERT(m_buffer, "not enough memory");
    }
}

bool QnMediaCyclicBuffer::resize(size_type size)
{
    value_type* buffer = (value_type*)qMallocAligned(size, m_align);
    NX_ASSERT(buffer, "not enough memory");

    if (buffer != nullptr)
    {
        qFreeAligned(m_buffer);
        m_buffer = buffer;
        m_maxSize = size;
    }

    return buffer != nullptr;
}

QnMediaCyclicBuffer::~QnMediaCyclicBuffer()
{
    qFreeAligned(m_buffer);
}

void QnMediaCyclicBuffer::push_back(const value_type* data, size_type size)
{
    insert(m_size, data, size);
}

void QnMediaCyclicBuffer::insert(size_type pos, const value_type* data, size_type size)
{
    NX_ASSERT(pos + size <= m_maxSize);
    size_type updPos = (m_offset + pos) % m_maxSize;
    int copySize = qMin(m_maxSize - updPos, size);
    memcpy(m_buffer + updPos, data, copySize);
    if (size > copySize)
        memcpy(m_buffer, data + copySize, size - copySize);
    if (pos + size > m_size)
        m_size = pos + size;
}

void QnMediaCyclicBuffer::pop_front(size_type size)
{
    NX_ASSERT(m_size >= size);

    m_size -= size;
    m_offset += size;
    if (m_offset >= m_maxSize)
        m_offset -= m_maxSize;
}

const QnMediaCyclicBuffer::value_type* QnMediaCyclicBuffer::unfragmentedData(size_type pos, size_type size)
{
    if (size == -1)
        size = m_size;
    NX_ASSERT(pos + size <= m_size);
    size_type reqPos = (m_offset + pos) % m_maxSize;
    if (reqPos + size > m_maxSize) {
        reallocateBuffer();
        reqPos = pos;
    }
    return m_buffer + reqPos;
}

std::vector<QnMediaCyclicBuffer::Range> QnMediaCyclicBuffer::fragmentedData(size_type pos, size_type size) const
{
    if (size == -1)
        size = m_size;
    NX_ASSERT(pos + size <= m_size);

    std::vector<QnMediaCyclicBuffer::Range> result;
    if (m_size == 0 || size == 0)
        return result;
    size_type reqPos = (m_offset + pos) % m_maxSize;
    size_type frontDataSize = qMin(size, m_maxSize - reqPos);
    result.push_back(Range(m_buffer + reqPos, frontDataSize));
    if (size > frontDataSize)
        result.push_back(Range(m_buffer, size - frontDataSize));
    return result;
}

void QnMediaCyclicBuffer::reallocateBuffer()
{
    if (m_offset + m_size <= m_maxSize) {
        memmove(m_buffer, m_buffer + m_offset, m_size);
    }
    else {
        int frontDataSize = (m_offset + m_size) - m_maxSize;
        int backDataSize = m_maxSize - m_offset;

        if (frontDataSize < backDataSize) {
            value_type* tmpBuffer = new value_type[frontDataSize];
            memcpy(tmpBuffer, m_buffer, frontDataSize);
            memmove(m_buffer, m_buffer + m_offset, backDataSize);
            memcpy(m_buffer + backDataSize, tmpBuffer, frontDataSize);
            delete [] tmpBuffer;
        }
        else {
            value_type* tmpBuffer = new value_type[backDataSize];
            memcpy(tmpBuffer, m_buffer + m_offset, backDataSize);
            memmove(m_buffer + backDataSize, m_buffer, m_size - backDataSize);
            memcpy(m_buffer, tmpBuffer, backDataSize);
            delete [] tmpBuffer;
        }
    }
    m_offset = 0;
}
