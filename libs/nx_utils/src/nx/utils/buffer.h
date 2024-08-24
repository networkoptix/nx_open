// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <compare>
#include <cstring>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include <boost/functional/hash/hash.hpp>

#include <QtCore/QByteArray>

#include <nx/reflect/instrument.h>
#include <nx/utils/log/assert.h>

#include "base64.h"
#include "type_utils.h"

#define NX_BUFFER_IMPL

namespace nx {

/**
 * BasicBuffer class, compatible with std::basic_string. Stores data inside as std::basic_string or QByteArray.
 * The latter allows instantiating this class from QByteArray.
 */
template<typename CharType>
class BasicBuffer
{
public:
    using size_type = std::size_t;
    using value_type = CharType;

    static constexpr auto npos = std::basic_string<value_type>::npos;

    BasicBuffer() = default;

    BasicBuffer(size_type count, value_type ch);
    BasicBuffer(const value_type* buf, size_type count);
    BasicBuffer(const value_type* buf);
    BasicBuffer(const std::basic_string<value_type>& other);
    BasicBuffer(std::basic_string<value_type>&& other);
    BasicBuffer(const QByteArray& other);
    BasicBuffer(QByteArray&& other);
    BasicBuffer(const std::basic_string_view<value_type>& other);

    BasicBuffer(const BasicBuffer&);
    BasicBuffer(BasicBuffer&&) noexcept;

    void assign(const value_type* buf, size_type count);
    void assign(const value_type* buf);
    void assign(const std::basic_string<value_type>& other);
    void assign(std::basic_string<value_type>&& other);
    void assign(const QByteArray& other);
    void assign(QByteArray&& other);
    void assign(const std::basic_string_view<value_type>& other);

    BasicBuffer& operator=(const BasicBuffer&);
    BasicBuffer& operator=(BasicBuffer&&) noexcept;

    BasicBuffer& operator=(const value_type* buf);
    BasicBuffer& operator=(const std::basic_string<value_type>& other);
    BasicBuffer& operator=(std::basic_string<value_type>&& other);
    BasicBuffer& operator=(const QByteArray& other);
    BasicBuffer& operator=(QByteArray&& other);
    BasicBuffer& operator=(const std::basic_string_view<value_type>& other);

    std::strong_ordering operator<=>(const BasicBuffer&) const;

    value_type& at(size_type pos);
    const value_type& at(size_type pos) const;

    value_type& operator[](size_type pos);
    const value_type& operator[](size_type pos) const;

    value_type front() const;
    value_type& front();

    value_type back() const;
    value_type& back();

    const value_type* data() const;
    value_type* data();

    auto begin() const { return data(); }
    auto begin() { return data(); }
    auto cbegin() const { return data(); }

    auto end() const { return data() + size(); }
    auto end() { return data() + size(); }
    auto cend() const { return data() + size(); }

    operator std::basic_string_view<value_type>() const;

    BasicBuffer substr(size_type pos = 0, size_type count = npos) const;

    bool empty() const;
    void resize(size_type count, value_type ch = value_type());
    size_type size() const;
    void reserve(size_type count);
    size_type capacity() const;

    /**
     * NOTE: Resets capacity to 0.
     */
    void clear();

    BasicBuffer& append(const value_type* buf, size_type count);
    BasicBuffer& append(const value_type* buf);
    BasicBuffer& append(value_type ch);
    BasicBuffer& append(const std::basic_string<value_type>& other);
    BasicBuffer& append(const QByteArray& other);
    BasicBuffer& append(const std::basic_string_view<value_type>& other);

    BasicBuffer& operator+=(const value_type* buf);
    BasicBuffer& operator+=(value_type ch);
    BasicBuffer& operator+=(const std::basic_string<value_type>& other);
    BasicBuffer& operator+=(const QByteArray& other);
    BasicBuffer& operator+=(const std::basic_string_view<value_type>& other);

    BasicBuffer& erase(size_type pos = 0, size_type count = npos);

    bool starts_with(const BasicBuffer& other) const;
    bool starts_with(const std::basic_string<value_type>& other) const;
    bool starts_with(const value_type* other) const;
    bool starts_with(const value_type* buf, size_type count) const;
    bool starts_with(const std::basic_string_view<value_type>& other) const;

    bool ends_with(const BasicBuffer& other) const;
    bool ends_with(const std::basic_string<value_type>& other) const;
    bool ends_with(const value_type* other) const;
    bool ends_with(const value_type* buf, size_type count) const;
    bool ends_with(const std::basic_string_view<value_type>& other) const;

    void swap(BasicBuffer& other);

    /**
     * Compares this with other lexicographically.
     * @return < 0 if this is less than other. > 0 if this is greater. 0 if equal.
     */
    int compare(const BasicBuffer& other) const;
    int compare(const std::basic_string<value_type>& other) const;
    int compare(const value_type* other) const;
    int compare(const value_type* buf, size_type count) const;
    int compare(const std::basic_string_view<value_type>& other) const;
    int compare(const QByteArray& other) const;

    size_type find(const std::basic_string<value_type>& str, size_type pos = 0) const;
    size_type find(value_type ch, size_type pos = 0) const;
    size_type find(const std::basic_string_view<value_type>& str, size_type pos = 0) const;
    size_type find(const value_type* str, size_type pos = 0) const;

    size_type rfind(const std::basic_string<value_type>& str, size_type pos = npos) const;
    size_type rfind(value_type ch, size_type pos = npos) const;
    size_type rfind(const std::basic_string_view<value_type>& str, size_type pos = npos) const;
    size_type rfind(const value_type* str, size_type pos = npos) const;

    size_type find_first_of(const std::basic_string<value_type>& str, size_type pos = 0) const;
    size_type find_first_of(value_type ch, size_type pos = 0) const;
    size_type find_first_of(const std::basic_string_view<value_type>& str, size_type pos = 0) const;
    size_type find_first_of(const value_type* str, size_type pos = 0) const;

    size_type find_first_not_of(const std::basic_string<value_type>& str, size_type pos = 0) const;
    size_type find_first_not_of(value_type ch, size_type pos = 0) const;
    size_type find_first_not_of(const std::basic_string_view<value_type>& str, size_type pos = 0) const;
    size_type find_first_not_of(const value_type* str, size_type pos = 0) const;

    size_type find_last_of(const std::basic_string<value_type>& str, size_type pos = npos) const;
    size_type find_last_of(value_type ch, size_type pos = npos) const;
    size_type find_last_of(const std::basic_string_view<value_type>& str, size_type pos = npos) const;
    size_type find_last_of(const value_type* str, size_type pos = npos) const;

    size_type find_last_not_of(const std::basic_string<value_type>& str, size_type pos = npos) const;
    size_type find_last_not_of(value_type ch, size_type pos = npos) const;
    size_type find_last_not_of(const std::basic_string_view<value_type>& str, size_type pos = npos) const;
    size_type find_last_not_of(const value_type* str, size_type pos = npos) const;

    template<
        typename String,
        typename = decltype(std::declval<String>().data()),
        typename = decltype(std::declval<String>().size())
    >
    BasicBuffer& replace(size_type pos, size_type count, const String&);

    BasicBuffer& replace(size_type pos, size_type count, const CharType*);

    template<typename T>
    bool contains(const T& str) const;

    std::basic_string<CharType> toStdString() const;
    std::string toString() const;
    QByteArray toByteArray() const;

    /**
     * @return QByteArray::fromRawData(data(), size()).
     */
    QByteArray toRawByteArray() const { return QByteArray::fromRawData(data(), (int) size()); }

    /**
     * If the data is stored as std::basic_string, then moves that string outside leaving the buffer empty.
     */
    std::basic_string<CharType> takeStdString();

    /**
     * If the data is stored as QByteArray, then moves that QByteArray outside leaving the buffer empty.
     */
    QByteArray takeByteArray();

    std::string toBase64() const;
    static BasicBuffer fromBase64(const std::string_view& str);

private:
    /** Enough to store a text representation of a guid. */
    static constexpr size_type kPreallocatedBufSize = 40;

    std::optional<std::basic_string<value_type>> m_str;
    std::optional<QByteArray> m_qByteArray;
    char m_buf[kPreallocatedBufSize + 1]{0};

    value_type* m_data = m_buf;
    size_type m_size = 0;
    size_type m_capacity = kPreallocatedBufSize;

    void moveDataToStr();
};

NX_REFLECTION_TAG_TEMPLATE_TYPE(BasicBuffer, useStringConversionForSerialization)

//-------------------------------------------------------------------------------------------------

inline std::string toBase64(const BasicBuffer<char>& buf)
{
    return buf.toBase64();
}

inline BasicBuffer<char> fromBase64(const std::string_view& str)
{
    return BasicBuffer<char>::fromBase64(str);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
QByteArray toByteArray(const BasicBuffer<CharType>& buf)
{
    return buf.toByteArray();
}

template<typename CharType>
QByteArray toByteArray(BasicBuffer<CharType>&& buf)
{
    return buf.takeByteArray();
}

template<typename CharType>
std::basic_string<CharType> toStdString(const BasicBuffer<CharType>& buf)
{
    return buf.toStdString();
}

template<typename CharType>
std::basic_string<CharType> toStdString(BasicBuffer<CharType>&& buf)
{
    return buf.takeStdString();
}

//-------------------------------------------------------------------------------------------------
// BasicBuffer implementation

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(size_type count, value_type ch)
{
    resize(count, ch);
}

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(const value_type* buf, size_type count)
{
    assign(buf, count);
}

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(const value_type* buf)
{
    assign(buf);
}

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(const std::basic_string<value_type>& other)
{
    assign(other);
}

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(std::basic_string<value_type>&& other)
{
    assign(std::move(other));
}

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(const QByteArray& other)
{
    assign(other);
}

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(QByteArray&& other)
{
    assign(std::move(other));
}

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(const std::basic_string_view<value_type>& other)
{
    assign(other);
}

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(const BasicBuffer& other)
{
    assign(other);
}

template<typename CharType>
BasicBuffer<CharType>::BasicBuffer(BasicBuffer&& other) noexcept
{
    *this = std::move(other);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
void BasicBuffer<CharType>::assign(const value_type* buf, size_type count)
{
    clear();

    if (count <= kPreallocatedBufSize)
    {
        memcpy(m_buf, buf, count);
        m_buf[count] = value_type();
        m_data = m_buf;
        m_size = count;
        m_capacity = kPreallocatedBufSize;
    }
    else
    {
        m_str.emplace(buf, count);

        m_data = m_str->data();
        m_size = m_str->size();
        m_capacity = m_str->capacity();
    }
}

template<typename CharType>
void BasicBuffer<CharType>::assign(const value_type* buf)
{
    assign(buf, buf ? std::strlen(buf) : 0);
}

template<typename CharType>
void BasicBuffer<CharType>::assign(const std::basic_string<value_type>& other)
{
    clear();

    assign(other.data(), other.size());
}

template<typename CharType>
void BasicBuffer<CharType>::assign(std::basic_string<value_type>&& other)
{
    clear();

    m_str.emplace(std::move(other));

    m_data = m_str->data();
    m_size = m_str->size();
    m_capacity = m_str->capacity();
}

template<typename CharType>
void BasicBuffer<CharType>::assign(const QByteArray& other)
{
    // Invoking assign(QByteArray&&) overload.
    assign(QByteArray(other));
}

template<typename CharType>
void BasicBuffer<CharType>::assign(QByteArray&& other)
{
    clear();

    m_qByteArray.emplace(std::move(other));

    m_data = m_qByteArray->data();
    m_size = m_qByteArray->size();
    m_capacity = m_qByteArray->capacity();
}

template<typename CharType>
void BasicBuffer<CharType>::assign(const std::basic_string_view<value_type>& other)
{
    assign(other.data(), other.size());
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator=(const BasicBuffer& other)
{
    assign(other);
    return *this;
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator=(BasicBuffer&& other) noexcept
{
    m_str = std::move(other.m_str);
    m_qByteArray = std::move(other.m_qByteArray);
    memcpy(m_buf, other.m_buf, sizeof(m_buf));

    m_size = other.m_size;
    m_capacity = other.m_capacity;

    if (m_str)
        m_data = m_str->data();
    else if (m_qByteArray)
        m_data = m_qByteArray->data();
    else
        m_data = m_buf;

    // Ensure that the state of other is reset to expected values.
    other.clear();

    return *this;
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator=(const value_type* buf)
{
    assign(buf);
    return *this;
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator=(const std::basic_string<value_type>& other)
{
    assign(other);
    return *this;
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator=(std::basic_string<value_type>&& other)
{
    assign(std::move(other));
    return *this;
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator=(const QByteArray& other)
{
    assign(other);
    return *this;
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator=(QByteArray&& other)
{
    assign(std::move(other));
    return *this;
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator=(const std::basic_string_view<value_type>& other)
{
    assign(other);
    return *this;
}

template<typename CharType>
std::strong_ordering BasicBuffer<CharType>::operator<=>(const BasicBuffer& right) const
{
    const int cmp = compare(right);
    return cmp == 0 ? std::strong_ordering::equal
        : (cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
typename BasicBuffer<CharType>::value_type& BasicBuffer<CharType>::at(size_type pos)
{
    return *(data() + pos);
}

template<typename CharType>
const typename BasicBuffer<CharType>::value_type& BasicBuffer<CharType>::at(size_type pos) const
{
    return *(data() + pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::value_type& BasicBuffer<CharType>::operator[](size_type pos)
{
    return at(pos);
}

template<typename CharType>
const typename BasicBuffer<CharType>::value_type& BasicBuffer<CharType>::operator[](size_type pos) const
{
    return at(pos);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
typename BasicBuffer<CharType>::value_type BasicBuffer<CharType>::front() const
{
    return *m_data;
}

template<typename CharType>
typename BasicBuffer<CharType>::value_type& BasicBuffer<CharType>::front()
{
    return *m_data;
}

template<typename CharType>
typename BasicBuffer<CharType>::value_type BasicBuffer<CharType>::back() const
{
    return *(m_data + m_size);
}

template<typename CharType>
typename BasicBuffer<CharType>::value_type& BasicBuffer<CharType>::back()
{
    return *(m_data + m_size);
}

template<typename CharType>
const typename BasicBuffer<CharType>::value_type* BasicBuffer<CharType>::data() const
{
    return m_data;
}

template<typename CharType>
typename BasicBuffer<CharType>::value_type* BasicBuffer<CharType>::data()
{
    return m_data;
}

template<typename CharType>
BasicBuffer<CharType>::operator std::basic_string_view<CharType>() const
{
    return std::basic_string_view<value_type>(m_data, m_size);
}

template<typename CharType>
bool BasicBuffer<CharType>::empty() const
{
    return size() == 0;
}

template<typename CharType>
BasicBuffer<CharType> BasicBuffer<CharType>::substr(size_type pos, size_type count) const
{
    if (pos > size())
    {
        throw std::out_of_range("Index " + std::to_string(pos) +
            " is out of BasicBuffer of size " + std::to_string(size()));
    }

    return BasicBuffer<CharType>(
        data() + pos,
        count == npos ? size() - pos : std::min(count, size() - pos));
}

template<typename CharType>
void BasicBuffer<CharType>::resize(size_type count, value_type ch)
{
    if (count > m_capacity)
    {
        moveDataToStr();

        if (!m_str)
            m_str.emplace();
    }

    if (m_str)
    {
        m_str->resize(count, ch);
        m_data = m_str->data();
        m_capacity = m_str->capacity();
    }
    else if (m_qByteArray)
    {
        m_qByteArray->resize((int) count);
        m_data = m_qByteArray->data();
        m_capacity = m_qByteArray->capacity();
    }
    else
    {
        if (count < sizeof(m_buf) / sizeof(*m_buf))
        {
            m_buf[count] = value_type();
        }
        else
        {
            // NOTE: This assert is actually unreachable ("if (count > m_capacity)" condition guarantees that),
            // but without it gcc produces a compilation warning (out of bounds in "m_buf[count]").
            NX_ASSERT(false);
        }
    }

    // NOTE: m_size still holds the old value.

    if (count > m_size)
        memset(m_data + m_size, ch, (count - m_size) * sizeof(value_type));

    m_size = count;
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::size() const
{
    return m_size;
}

template<typename CharType>
void BasicBuffer<CharType>::reserve(size_type count)
{
    if (count <= m_capacity)
        return;

    moveDataToStr();

    if (!m_str)
        m_str.emplace();

    m_str->reserve(count);
    m_data = m_str->data();
    m_capacity = m_str->capacity();
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::capacity() const
{
    return m_capacity;
}

template<typename CharType>
void BasicBuffer<CharType>::clear()
{
    // TODO: #akolesnikov Should (but not MUST) leave capacity unchanged.

    m_str = std::nullopt;
    m_qByteArray = std::nullopt;
    m_size = 0;
    m_capacity = kPreallocatedBufSize;
    m_data = m_buf;
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::append(const value_type* buf, size_type count)
{
    if (count == 0)
        return *this;

    if (m_capacity < size() + count)
    {
        moveDataToStr();
        if (!m_str)
            m_str.emplace();
    }

    if (m_str)
    {
        m_str->append(buf, count);
        m_data = m_str->data();
        m_capacity = m_str->capacity();
    }
    else if (m_qByteArray)
    {
        m_qByteArray->append(buf, (int) count);
        // NOTE: m_qByteArray->data() may still change because of implicit sharing.
        m_data = m_qByteArray->data();
        m_capacity = m_qByteArray->capacity();
    }
    else
    {
        memcpy(m_buf + m_size, buf, count);
        m_buf[m_size + count] = value_type();
    }

    m_size += count;

    return *this;
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::append(const value_type* buf)
{
    return append(buf, std::strlen(buf));
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::append(value_type ch)
{
    return append(&ch, 1);
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::append(const std::basic_string<value_type>& other)
{
    return append(other.data(), other.size());
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::append(const QByteArray& other)
{
    return append(other.data(), other.size());
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::append(const std::basic_string_view<value_type>& other)
{
    return append(other.data(), other.size());
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator+=(const value_type* buf)
{
    return append(buf);
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator+=(value_type ch)
{
    return append(ch);
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator+=(const std::basic_string<value_type>& other)
{
    return append(other);
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator+=(const QByteArray& other)
{
    return append(other);
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::operator+=(const std::basic_string_view<value_type>& other)
{
    return append(other);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::erase(size_type pos, size_type count)
{
    if (count == 0)
        return *this;

    if (pos > size())
    {
        throw std::out_of_range("Index " + std::to_string(pos) +
            " is out of BasicBuffer of size " + std::to_string(size()));
    }

    if (m_str)
    {
        m_str->erase(pos, count);
        m_data = m_str->data();
        m_size = m_str->size();
        m_capacity = m_str->capacity();
    }
    else if (m_qByteArray)
    {
        m_qByteArray->remove((int) pos, (int) (count == npos ? m_size - pos : count));
        m_data = m_qByteArray->data();
        m_size = m_qByteArray->size();
        m_capacity = m_qByteArray->capacity();
    }
    else
    {
        NX_ASSERT(m_data == m_buf);
        if (count == npos)
        {
            m_size = pos;
        }
        else
        {
            memmove(m_data + pos, m_data + pos + count, m_size - (pos + count));
            m_size -= count;
        }
    }

    return *this;
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
bool BasicBuffer<CharType>::starts_with(const BasicBuffer& other) const
{
    return starts_with(other.data(), other.size());
}

template<typename CharType>
bool BasicBuffer<CharType>::starts_with(const std::basic_string<value_type>& other) const
{
    return starts_with(other.data(), other.size());
}

template<typename CharType>
bool BasicBuffer<CharType>::starts_with(const value_type* other) const
{
    return starts_with(other, std::strlen(other));
}

template<typename CharType>
bool BasicBuffer<CharType>::starts_with(const value_type* buf, size_type count) const
{
    if (count > m_size)
        return false;

    return std::memcmp(m_data, buf, count) == 0;
}

template<typename CharType>
bool BasicBuffer<CharType>::starts_with(const std::basic_string_view<value_type>& other) const
{
    return starts_with(other.data(), other.size());
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
bool BasicBuffer<CharType>::ends_with(const BasicBuffer& other) const
{
    return ends_with(other.data(), other.size());
}

template<typename CharType>
bool BasicBuffer<CharType>::ends_with(const std::basic_string<value_type>& other) const
{
    return ends_with(other.data(), other.size());
}

template<typename CharType>
bool BasicBuffer<CharType>::ends_with(const value_type* other) const
{
    return ends_with(other, std::strlen(other));
}

template<typename CharType>
bool BasicBuffer<CharType>::ends_with(const value_type* buf, size_type count) const
{
    if (count > m_size)
        return false;

    return std::memcmp(m_data + m_size - count, buf, count) == 0;
}

template<typename CharType>
bool BasicBuffer<CharType>::ends_with(const std::basic_string_view<value_type>& other) const
{
    return ends_with(other.data(), other.size());
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
void BasicBuffer<CharType>::swap(BasicBuffer<CharType>& other)
{
    std::swap(m_str, other.m_str);
    std::swap(m_qByteArray, other.m_qByteArray);
    std::swap(m_buf, other.m_buf);
    std::swap(m_size, other.m_size);
    std::swap(m_capacity, other.m_capacity);

    if (m_str)
        m_data = m_str->data();
    else if (m_qByteArray)
        m_data = m_qByteArray->data();
    else
        m_data = m_buf;

    if (other.m_str)
        other.m_data = other.m_str->data();
    else if (other.m_qByteArray)
        other.m_data = other.m_qByteArray->data();
    else
        other.m_data = other.m_buf;
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
int BasicBuffer<CharType>::compare(const BasicBuffer& other) const
{
    return compare(std::basic_string_view<value_type>(other.data(), other.size()));
}

template<typename CharType>
int BasicBuffer<CharType>::compare(const std::basic_string<value_type>& other) const
{
    return compare(std::basic_string_view<value_type>(other.data(), other.size()));
}

template<typename CharType>
int BasicBuffer<CharType>::compare(const value_type* other) const
{
    return compare(std::basic_string_view<value_type>(other));
}

template<typename CharType>
int BasicBuffer<CharType>::compare(const value_type* buf, size_type count) const
{
    return compare(std::basic_string_view<value_type>(buf, count));
}

template<typename CharType>
int BasicBuffer<CharType>::compare(const std::basic_string_view<value_type>& other) const
{
    return std::basic_string_view<value_type>(data(), size()).compare(other);
}

template<typename CharType>
int BasicBuffer<CharType>::compare(const QByteArray& other) const
{
    return compare(std::basic_string_view<value_type>(other.data(), other.size()));
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find(
    const std::basic_string<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find(
    value_type ch, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find(ch, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find(
    const std::basic_string_view<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find(
    const value_type* str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find(str, pos);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::rfind(
    const std::basic_string<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).rfind(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::rfind(
    value_type ch, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).rfind(ch, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::rfind(
    const std::basic_string_view<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).rfind(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::rfind(
    const value_type* str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).rfind(str, pos);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_first_of(
    const std::basic_string<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_first_of(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_first_of(
    value_type ch, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_first_of(ch, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_first_of(
    const std::basic_string_view<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_first_of(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_first_of(
    const value_type* str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_first_of(str, pos);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_first_not_of(
    const std::basic_string<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_first_not_of(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_first_not_of(
    value_type ch, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_first_not_of(ch, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_first_not_of(
    const std::basic_string_view<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_first_not_of(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_first_not_of(
    const value_type* str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_first_not_of(str, pos);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_last_of(
    const std::basic_string<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_last_of(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_last_of(
    value_type ch, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_last_of(ch, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_last_of(
    const std::basic_string_view<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_last_of(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_last_of(
    const value_type* str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_last_of(str, pos);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_last_not_of(
    const std::basic_string<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_last_not_of(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_last_not_of(
    value_type ch, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_last_not_of(ch, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_last_not_of(
    const std::basic_string_view<value_type>& str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_last_not_of(str, pos);
}

template<typename CharType>
typename BasicBuffer<CharType>::size_type BasicBuffer<CharType>::find_last_not_of(
    const value_type* str, size_type pos) const
{
    return std::basic_string_view<value_type>(data(), size()).find_last_not_of(str, pos);
}

template<typename CharType>
template<typename String, typename, typename>
BasicBuffer<CharType>& BasicBuffer<CharType>::replace(
    size_type pos, size_type count,
    const String& str)
{
    if (count == npos)
        count = size() - pos;

    const auto newSize = size() + str.size() - count;
    if (newSize > size())
        resize(newSize);
    memmove(data() + pos + str.size(), data() + pos + count, size() - pos - count);
    memcpy(data() + pos, str.data(), str.size());
    resize(newSize);

    return *this;
}

template<typename CharType>
BasicBuffer<CharType>& BasicBuffer<CharType>::replace(
    size_type pos, size_type count,
    const CharType* str)
{
    return replace(pos, count, std::basic_string_view<CharType>(str));
}

template<typename CharType>
template<typename T>
bool BasicBuffer<CharType>::contains(const T& str) const
{
    return find(str) != npos;
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
std::basic_string<CharType> BasicBuffer<CharType>::toStdString() const
{
    return std::basic_string<CharType>(data(), size());
}

template<typename CharType>
std::string BasicBuffer<CharType>::toString() const
{
    return toStdString();
}

template<typename CharType>
QByteArray BasicBuffer<CharType>::toByteArray() const
{
    if (m_qByteArray)
        return *m_qByteArray;

    return QByteArray(data(), (int) size());
}

template<typename CharType>
std::basic_string<CharType> BasicBuffer<CharType>::takeStdString()
{
    std::basic_string<CharType> result;

    if (m_str)
        result = std::exchange(*m_str, {});
    else
        result = std::basic_string<CharType>(data(), size());

    clear();

    return result;
}

template<typename CharType>
QByteArray BasicBuffer<CharType>::takeByteArray()
{
    QByteArray result;

    if (m_qByteArray)
        result = std::exchange(*m_qByteArray, {});
    else
        result = QByteArray(data(), (int) size());

    clear();

    return result;
}

template<typename CharType>
std::string BasicBuffer<CharType>::toBase64() const
{
    std::string result;

    result.resize((std::size_t) utils::toBase64(data(),
        (int) size() * sizeof(CharType), nullptr, 0));

    result.resize((std::size_t) utils::toBase64(data(),
        (int)size() * sizeof(CharType), result.data(), (int) result.size()));

    return result;
}

template<typename CharType>
BasicBuffer<CharType> BasicBuffer<CharType>::fromBase64(const std::string_view& str)
{
    BasicBuffer<char> result;

    const auto byteCount = (std::size_t) utils::fromBase64(str.data(), (int) str.size(), nullptr, 0);
    result.resize((byteCount + sizeof(CharType) - 1) / sizeof(CharType));

    result.resize(utils::fromBase64(str.data(), (int) str.size(),
        result.data(), (int) result.size() * sizeof(CharType)));
    return result;
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
void BasicBuffer<CharType>::moveDataToStr()
{
    if (m_str)
        return;

    if (m_qByteArray)
    {
        m_str.emplace();
        m_str->reserve(m_capacity);
        m_str->assign(m_qByteArray->data(), m_qByteArray->size());
        m_data = m_str->data();
        m_qByteArray = std::nullopt;
        return;
    }

    if (m_size > 0)
    {
        NX_ASSERT(m_size <= kPreallocatedBufSize && m_data == m_buf);
        m_str.emplace();
        m_str->reserve(m_capacity);
        m_str->assign(m_buf, m_size);
        m_data = m_str->data();
        return;
    }
}

//-------------------------------------------------------------------------------------------------

#if defined(NX_BUFFER_IMPL)

using Buffer = BasicBuffer<char>;

inline std::ostream& operator<<(std::ostream& os, const Buffer& buf)
{
    return os << buf.toStdString();
}

/**
 * Extends std::string_view by introducing implicit conversion from QByteArray.
 */
class ConstBufferRefType:
    public std::string_view
{
    using base_type = std::string_view;

public:
    ConstBufferRefType() noexcept = default;

    ConstBufferRefType(const std::string_view& str):
        base_type(str)
    {
    }

    ConstBufferRefType(const char* str, std::size_t count):
        base_type(str, count)
    {
    }

    ConstBufferRefType(const char* str):
        base_type(str)
    {
    }

    /**
     * NOTE: This constructor works with any type that has "const char* data()" and "size()".
     * E.g., QByteArray.
     */
    template<typename T>
    ConstBufferRefType(
        const T& str,
        std::enable_if_t<
            nx::utils::IsConvertibleToStringViewV<T> &&
            !std::is_same_v<std::decay_t<T>, std::string_view>>* = nullptr)
        :
        base_type(str.data(), (std::size_t) str.size())
    {
    }

    QByteArray toRawByteArray() const
    {
        return QByteArray::fromRawData(data(), (int) size());
    }
};

#else

/**
 * Some effective buffer is required. Following is desired:
 * - substr O(1) complexity
 * - pop_front, pop_back O(1) complexity
 * - concatenation O(1) complexity. This implies readiness for readv and writev system calls
 * - buffer should implicit sharing but still minimize atomic operations where they not required
 * Currently, using std::string, but fully new implementation will be provided one day...
 */
using Buffer = QByteArray;

#endif

//-------------------------------------------------------------------------------------------------

inline std::string_view toBufferView(const QByteArray& data)
{
    return std::string_view(data.data(), data.size());
}

//-------------------------------------------------------------------------------------------------

template<typename CharType, typename Other>
bool operator==(const nx::BasicBuffer<CharType>& left, const Other& right)
{
    return left.compare(right) == 0;
}

template<typename CharType, typename Other,
    typename X = std::enable_if_t<!std::is_same_v<Other, nx::BasicBuffer<CharType>>>
>
bool operator==(const Other& left, const nx::BasicBuffer<CharType>& right)
{
    return right.compare(left) == 0;
}

template<typename CharType, typename Other>
bool operator!=(const nx::BasicBuffer<CharType>& left, const Other& right)
{
    return left.compare(right) != 0;
}

template<typename CharType, typename Other,
    typename X = std::enable_if_t<!std::is_same_v<Other, nx::BasicBuffer<CharType>>>
>
bool operator!=(const Other& left, const nx::BasicBuffer<CharType>& right)
{
    return right.compare(left) != 0;
}

template<typename CharType, typename Other>
bool operator<(const nx::BasicBuffer<CharType>& left, const Other& right)
{
    return left.compare(right) < 0;
}

template<typename CharType, typename Other,
    typename X = std::enable_if_t<!std::is_same_v<Other, nx::BasicBuffer<CharType>>>
>
bool operator<(const Other& left, const nx::BasicBuffer<CharType>& right)
{
    return right.compare(left) > 0;
}

template<typename CharType, typename Other>
bool operator<=(const nx::BasicBuffer<CharType>& left, const Other& right)
{
    return left.compare(right) <= 0;
}

template<typename CharType, typename Other,
    typename X = std::enable_if_t<!std::is_same_v<Other, nx::BasicBuffer<CharType>>>
>
bool operator<=(const Other& left, const nx::BasicBuffer<CharType>& right)
{
    return right.compare(left) >= 0;
}

template<typename CharType, typename Other>
bool operator>(const nx::BasicBuffer<CharType>& left, const Other& right)
{
    return left.compare(right) > 0;
}

template<typename CharType, typename Other,
    typename X = std::enable_if_t<!std::is_same_v<Other, nx::BasicBuffer<CharType>>>
>
bool operator>(const Other& left, const nx::BasicBuffer<CharType>& right)
{
    return right.compare(left) < 0;
}

template<typename CharType, typename Other>
bool operator>=(const nx::BasicBuffer<CharType>& left, const Other& right)
{
    return left.compare(right) >= 0;
}

template<typename CharType, typename Other,
    typename X = std::enable_if_t<!std::is_same_v<Other, nx::BasicBuffer<CharType>>>
>
bool operator>=(const Other& left, const nx::BasicBuffer<CharType>& right)
{
    return right.compare(left) <= 0;
}

//-------------------------------------------------------------------------------------------------

template<typename CharType>
BasicBuffer<CharType> operator+(const BasicBuffer<CharType>& one, const BasicBuffer<CharType>& two)
{
    BasicBuffer<CharType> result;
    result.reserve(one.size() + std::size(two));
    result.append(one);
    result.append(two);
    return result;
}

template<typename CharType, typename Other>
BasicBuffer<CharType> operator+(const BasicBuffer<CharType>& one, const Other& two)
{
    BasicBuffer<CharType> result;
    result.reserve(one.size() + std::size(two));
    result.append(one);
    result.append(two);
    return result;
}

template<typename CharType, typename Other>
BasicBuffer<CharType> operator+(const BasicBuffer<CharType>& one, const CharType* two)
{
    BasicBuffer<CharType> result;
    result.reserve(one.size() + std::strlen(two));
    result.append(one);
    result.append(two);
    return result;
}

template<typename CharType, typename Other>
BasicBuffer<CharType> operator+(const Other& one, const BasicBuffer<CharType>& two)
{
    BasicBuffer<CharType> result;
    result.reserve(std::size(one) + two.size());
    result.append(one);
    result.append(two);
    return result;
}

template<typename CharType, typename Other>
BasicBuffer<CharType> operator+(const CharType* one, const BasicBuffer<CharType>& two)
{
    BasicBuffer<CharType> result;
    result.reserve(std::strlen(one) + two.size());
    result.append(one);
    result.append(two);
    return result;
}

} // namespace nx

namespace std {

template<typename CharType>
struct hash<nx::BasicBuffer<CharType>>
{
    size_t operator()(const nx::BasicBuffer<CharType>& buffer) const
    {
        return boost::hash_range(buffer.begin(), buffer.end());
    }
};

} // namespace std
