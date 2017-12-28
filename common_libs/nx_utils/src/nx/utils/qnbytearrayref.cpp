#include "qnbytearrayref.h"

#include <cstring>

#include <nx/utils/log/assert.h>

QnByteArrayConstRef::QnByteArrayConstRef():
    m_src(NULL),
    m_offset(0),
    m_count(0)
{
}

QnByteArrayConstRef::QnByteArrayConstRef(
    const QByteArray& src,
    size_type offset,
    size_type count)
    :
    m_src(&src),
    m_offset(offset),
    m_count(count == npos ? src.size() - offset : count)
{
    NX_ASSERT(m_offset <= (size_type)src.size());
    NX_ASSERT(m_count <= (size_type)src.size());
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

QnByteArrayConstRef QnByteArrayConstRef::mid(
    QnByteArrayConstRef::size_type offset,
    QnByteArrayConstRef::size_type count) const
{
    if (count == npos)
        count = m_count - offset;
    return QnByteArrayConstRef(*m_src, m_offset + offset, count);
}

bool QnByteArrayConstRef::isEmpty() const
{
    return m_count == 0;
}

int QnByteArrayConstRef::indexOf(char sep) const
{
    const value_type* sepPos = static_cast<const value_type*>(memchr(constData(), sep, size()));
    if (sepPos == NULL)
        return -1;
    return sepPos - constData();
}

bool QnByteArrayConstRef::startsWith(const_pointer str, size_type len) const
{
    if (len == npos)
        len = strlen(str);
    if (m_count < len)
        return false;
    return memcmp(data(), str, len) == 0;
}

bool QnByteArrayConstRef::startsWith(value_type ch) const
{
    return startsWith(&ch, 1);
}

uint QnByteArrayConstRef::toUInt() const
{
    return toByteArrayWithRawData().toUInt();
}

float QnByteArrayConstRef::toFloat() const
{
    return toByteArrayWithRawData().toFloat();
}

QnByteArrayConstRef::value_type QnByteArrayConstRef::front() const
{
    return m_src->at(static_cast<int>(m_offset));
}

QnByteArrayConstRef::value_type QnByteArrayConstRef::back() const
{
    return m_src->at(static_cast<int>(m_offset + m_count - 1));
}

QList<QnByteArrayConstRef> QnByteArrayConstRef::split(char sep) const
{
    QList<QnByteArrayConstRef> tokenList;
    const value_type* dataEnd = constData() + size();
    const value_type* curTokenStart = constData();
    for (; curTokenStart < dataEnd; )
    {
        const value_type* sepPos = static_cast<const value_type*>(
            memchr(curTokenStart, sep, dataEnd - curTokenStart));
        if (sepPos == NULL)
            break;
        tokenList.append(QnByteArrayConstRef(
            *m_src, m_offset + (curTokenStart - constData()), sepPos - curTokenStart));
        curTokenStart = sepPos + 1;
    }
    if (curTokenStart <= dataEnd)
        tokenList.append(QnByteArrayConstRef(
            *m_src, m_offset + curTokenStart - constData(), dataEnd - curTokenStart));
    return tokenList;
}

QnByteArrayConstRef QnByteArrayConstRef::trimmed(const value_type* charsToTrim) const
{
    QnByteArrayConstRef result(*this);
    while (result.m_count > 0)
    {
        if (strchr(charsToTrim, result.front()) != NULL)
        {
            ++result.m_offset;
            --result.m_count;
        }
        else
        {
            break;
        }
    }

    while (result.m_count > 0)
    {
        if (strchr(charsToTrim, result.back()) != NULL)
            --result.m_count;
        else
            break;
    }

    return result;
}

void QnByteArrayConstRef::pop_front(size_type count)
{
    if (count == npos)
    {
        clear();
        return;
    }

    NX_ASSERT(count <= m_count);

    m_offset += count;
    m_count -= count;
}

void QnByteArrayConstRef::clear()
{
    m_src = nullptr;
    m_offset = 0;
    m_count = 0;
}

bool QnByteArrayConstRef::isEqualCaseInsensitive(const char* str, size_t strLength) const
{
    if (strLength == (size_t)-1)
        strLength = strlen(str);
    if (isEmpty())
        return strLength == 0;
    if (size() != strLength)
        return false;
#ifdef _WIN32
    return strnicmp(constData(), str, strLength) == 0;
#else
    return strncasecmp(constData(), str, strLength) == 0;
#endif
}

const QnByteArrayConstRef::value_type& QnByteArrayConstRef::operator[](size_type index) const
{
    NX_ASSERT(index < m_count);
    return *(constData() + index);
}

QnByteArrayConstRef::operator QByteArray() const
{
    return m_src ? m_src->mid(m_offset, (int)m_count) : QByteArray();
}

QByteArray QnByteArrayConstRef::toByteArrayWithRawData() const
{
    return QByteArray::fromRawData(constData(), m_count);
}

//-------------------------------------------------------------------------------------------------
// Non-member operators.

QDebug& operator<<( QDebug& stream, const QnByteArrayConstRef& value)
{
    return stream << value.toByteArrayWithRawData();
}

bool operator==(const QnByteArrayConstRef& left, const QByteArray& right)
{
    if (left.size() != (QnByteArrayConstRef::size_type)right.size())
        return false;
    return memcmp(left.constData(), right.constData(), left.size()) == 0;
}

bool operator==(const QByteArray& left, const QnByteArrayConstRef& right)
{
    return right == left;
}

bool operator!=(const QnByteArrayConstRef& left, const QByteArray& right)
{
    return !(left == right);
}

bool operator!=(const QByteArray& left, const QnByteArrayConstRef& right)
{
    return !(left == right);
}

bool operator==(const QnByteArrayConstRef::const_pointer& left, const QnByteArrayConstRef& right)
{
    const size_t leftLen = strlen(left);
    if (leftLen != right.size())
        return false;
    return memcmp(left, right.constData(), leftLen) == 0;
}

bool operator!=(const QnByteArrayConstRef::const_pointer& left, const QnByteArrayConstRef& right)
{
    return !(left == right);
}

bool operator==(const QnByteArrayConstRef& left, const QnByteArrayConstRef::const_pointer& right)
{
    return right == left;
}

bool operator!=(const QnByteArrayConstRef& left, const QnByteArrayConstRef::const_pointer& right)
{
    return right != left;
}
