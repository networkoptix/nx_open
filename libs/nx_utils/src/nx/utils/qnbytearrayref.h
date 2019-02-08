#pragma once

#include <QtCore/QByteArray>

/**
 * Reference to substring in QByteArray object.
 * Introduced to minimize memory copying by using QnByteArrayConstRef instead of QByteArray::mid
 * Provides a few methods of QByteArray to use it instead.
 * NOTE: This class methods do not contain array boundary validation!
 */
class NX_UTILS_API QnByteArrayConstRef
{
public:
    typedef size_t size_type;
    typedef QByteArray::value_type value_type;
    typedef const QByteArray::value_type* const_pointer;
    typedef const QByteArray::value_type& const_reference;

    static const size_t npos = (size_t)-1;

    QnByteArrayConstRef();
    QnByteArrayConstRef(
        const QByteArray& src,
        size_type offset = 0,
        size_type count = npos);

    const value_type* data() const;
    const value_type* constData() const;
    size_type size() const;
    QnByteArrayConstRef mid(size_type offset, size_type count = npos) const;
    bool isEmpty() const;
    int indexOf(char sep) const;
    bool startsWith(const_pointer str, size_type len = npos) const;
    bool startsWith(value_type) const;
    uint toUInt() const;
    float toFloat() const;
    value_type front() const;
    value_type back() const;
    QList<QnByteArrayConstRef> split(char sep) const;
    /** Removes from front and back of the string any character of null-terminated string charsToTrim. */
    QnByteArrayConstRef trimmed(const value_type* charsToTrim = " \t") const;
    /**
    * Removes count elements in the front.
    * @param count If npos then all elements are removed.
    */
    void pop_front(size_type count = 1);
    void clear();

    bool isEqualCaseInsensitive(const char* str, size_t strLength = (size_t)-1) const;

    const value_type& operator[](size_type index) const;
    /** Constructs new QByteArray object by calling QByteArray::mid. */
    operator QByteArray() const;

    /** Constructs QByteArray using QByteArray::fromRawData(), so returned buffer is usually NOT NULL-terminated! */
    QByteArray toByteArrayWithRawData() const;

private:
    /** Using pointer here to allow operator= to be implemented. */
    const QByteArray* m_src;
    size_type m_offset;
    size_type m_count;
};

bool NX_UTILS_API operator==(const QnByteArrayConstRef& left, const QByteArray& right);
bool NX_UTILS_API operator!=(const QnByteArrayConstRef& left, const QByteArray& right);
bool NX_UTILS_API operator==(const QByteArray& left, const QnByteArrayConstRef& right);
bool NX_UTILS_API operator!=(const QByteArray& left, const QnByteArrayConstRef& right);

/**
 * @param left 0-terminated string.
 */
bool NX_UTILS_API operator==(
    const QnByteArrayConstRef::const_pointer& left,
    const QnByteArrayConstRef& right);

bool NX_UTILS_API operator!=(
    const QnByteArrayConstRef::const_pointer& left,
    const QnByteArrayConstRef& right);

bool NX_UTILS_API operator==(
    const QnByteArrayConstRef& left,
    const QnByteArrayConstRef::const_pointer& right);

bool NX_UTILS_API operator!=(
    const QnByteArrayConstRef& left,
    const QnByteArrayConstRef::const_pointer& right);
