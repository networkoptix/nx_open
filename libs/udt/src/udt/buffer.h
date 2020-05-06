#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

/**
 * Buffer with implicit sharing.
 */
template<typename ValueType>
class BasicBuffer
{
public:
    using size_type = std::size_t;
    using value_type = ValueType;

    static constexpr size_type npos = (size_type)-1;

    BasicBuffer() = default;
    BasicBuffer(std::size_t initialSize, ValueType ch);
    BasicBuffer(const value_type* data, size_type count);

    BasicBuffer(const BasicBuffer&) = default;
    BasicBuffer(BasicBuffer&&) = default;
    BasicBuffer& operator=(const BasicBuffer&) = default;
    BasicBuffer& operator=(BasicBuffer&&) = default;

    value_type* data();
    const value_type* data() const;
    size_type size() const;

    /**
     * NOTE: Decreasing size does not cause reallocation.
     */
    void resize(size_type newSize);

    void assign(const value_type* data, size_type count);

    /**
     * NOTE: No bounds checking is performed. So, if pos > size() behavior is undefined.
     */
    value_type& operator[](size_type pos);
    const value_type& operator[](size_type pos) const;

    /**
     * @return BasicBuffer that points to the existing buffer with offset.
     * So, data is not copied.
     */
    BasicBuffer substr(size_type offset, size_type count = npos) const;
    bool empty() const;

private:
    std::shared_ptr<ValueType> m_data;
    size_type m_offset = 0;
    size_type m_size = 0;

    // Substring initializer.
    BasicBuffer(
        std::shared_ptr<ValueType> data,
        std::size_t offset,
        std::size_t size);

    std::shared_ptr<ValueType> allocate(size_type size);
    void ensureExclusiveOwnershipOverData();
};

//-------------------------------------------------------------------------------------------------

template<typename ValueType>
BasicBuffer<ValueType>::BasicBuffer(std::size_t initialSize, ValueType ch):
    m_size(initialSize)
{
    if (m_size > 0)
    {
        m_data = allocate(m_size);
        memset(m_data.get(), ch, m_size * sizeof(ch));
    }
}

template<typename ValueType>
BasicBuffer<ValueType>::BasicBuffer(const value_type* data, size_type count)
{
    assign(data, count);
}

template<typename ValueType>
ValueType* BasicBuffer<ValueType>::data()
{
    ensureExclusiveOwnershipOverData();
    return m_data.get() + m_offset;
}

template<typename ValueType>
const ValueType* BasicBuffer<ValueType>::data() const
{
    return m_data.get() + m_offset;
}

template<typename ValueType>
typename BasicBuffer<ValueType>::size_type BasicBuffer<ValueType>::size() const
{
    return m_size;
}

template<typename ValueType>
void BasicBuffer<ValueType>::resize(size_type newSize)
{
    if (newSize <= m_size && newSize > 0)
    {
        m_size = newSize;
        return;
    }

    if (newSize > 0)
    {
        auto newData = allocate(newSize);
        memcpy(newData.get(), ((const BasicBuffer*)this)->data(), std::min(m_size, newSize));
        m_data = std::move(newData);
    }
    else
    {
        m_data.reset();
    }

    m_offset = 0;
    m_size = newSize;
}

template<typename ValueType>
void BasicBuffer<ValueType>::assign(const value_type* data, size_type count)
{
    m_data.reset();

    m_size = count;
    m_offset = 0;
    if (m_size > 0)
    {
        m_data = allocate(m_size);
        memcpy(m_data.get(), data, count);
    }
}

template<typename ValueType>
typename BasicBuffer<ValueType>::value_type&
BasicBuffer<ValueType>::operator[](size_type pos)
{
    return data()[pos];
}

template<typename ValueType>
const typename BasicBuffer<ValueType>::value_type&
BasicBuffer<ValueType>::operator[](size_type pos) const
{
    return data()[pos];
}

/**
 * NOTE: If the requested substring extends past the buffer then
 * substring will be shorter than count. So, the bound checking is performed.
 */
template<typename ValueType>
BasicBuffer<ValueType> BasicBuffer<ValueType>::substr(
    size_type offset, size_type count) const
{
    if (offset >= m_size)
    {
        return BasicBuffer();
    }
    else if (count == npos || offset + count > m_size)
    {
        count = m_size - offset;
    }

    return BasicBuffer(m_data, m_offset + offset, count);
}

template<typename ValueType>
bool BasicBuffer<ValueType>::empty() const
{
    return size() == 0;
}

template<typename ValueType>
BasicBuffer<ValueType>::BasicBuffer(
    std::shared_ptr<ValueType> data,
    std::size_t offset,
    std::size_t size)
    :
    m_data(std::move(data)),
    m_offset(offset),
    m_size(size)
{
}

template<typename ValueType>
void BasicBuffer<ValueType>::ensureExclusiveOwnershipOverData()
{
    if (m_data.use_count() > 1)
    {
        auto newData = allocate(m_size);
        memcpy(newData.get(), m_data.get() + m_offset, m_size);

        m_data = std::move(newData);
        m_offset = 0;
    }
}

template<typename ValueType>
std::shared_ptr<ValueType> BasicBuffer<ValueType>::allocate(size_type size)
{
    return std::shared_ptr<ValueType>(new ValueType[size], std::default_delete<ValueType[]>());
}

//using Buffer = BasicBuffer<char>;

// NOTE: Disabling implicit sharing to see if it resolves the problem VMS-18365.
using Buffer = std::string;
