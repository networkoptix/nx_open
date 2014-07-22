/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef QNBYTEARRAYREF_H
#define QNBYTEARRAYREF_H

#include <QtCore/QByteArray>


//!Reference to substring in \a QByteArray object
/*!
    Introduced to minimize memory copying by using \a QnByteArrayConstRef instead of \a QByteArray::mid
    Provides a few methods of \a QByteArray to use it instead.
    \note This class methods do not contain array boundary validation!
*/
class QnByteArrayConstRef
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
        size_type count = npos );

    const value_type* data() const;
    const value_type* constData() const;
    size_type size() const;
    QnByteArrayConstRef mid( size_type offset, size_type count = npos ) const;
    bool isEmpty() const;
    int indexOf(char sep) const;
    bool startsWith( const_pointer str, size_type len = npos ) const;
    uint toUInt() const;
    float toFloat() const;
    QList<QnByteArrayConstRef> split( char sep ) const;

    bool isEqualCaseInsensitive( const char* str, size_t strLength = (size_t)-1 ) const;

    const value_type& operator[]( size_type index ) const;
    //!Constructs new \a QByteArray object by calling \a QByteArray::mid
    operator QByteArray() const;

    //!Constructs QByteArray using \a QByteArray::fromRawData(), so returned buffer is usually NOT NULL-terminated!
    QByteArray toByteArrayWithRawData() const;

private:
    //!Using pointer here to allow operator= to be implemented
    const QByteArray* m_src;
    size_type m_offset;
    size_type m_count;
};

bool operator==( const QnByteArrayConstRef& left, const QByteArray& right );
bool operator!=( const QnByteArrayConstRef& left, const QByteArray& right );
bool operator==( const QByteArray& left, const QnByteArrayConstRef& right );
bool operator!=( const QByteArray& left, const QnByteArrayConstRef& right );
/*!
    \param left 0-terminated string
*/
bool operator==( const QnByteArrayConstRef::const_pointer& left, const QnByteArrayConstRef& right );
bool operator!=( const QnByteArrayConstRef::const_pointer& left, const QnByteArrayConstRef& right );
bool operator==( const QnByteArrayConstRef& left, const QnByteArrayConstRef::const_pointer& right );
bool operator!=( const QnByteArrayConstRef& left, const QnByteArrayConstRef::const_pointer& right );

#endif  //QNBYTEARRAYREF_H
