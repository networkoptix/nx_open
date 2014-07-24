/**********************************************************
* 28 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef NX_BUFFER_H
#define NX_BUFFER_H

#include <stdint.h>

#include <limits>

#include <QtCore/QByteArray>

#ifdef _WIN32
#undef max
#undef min
#endif


namespace nx
{
    class Buffer
    :
        public QByteArray
    {
    public:
        static const size_t npos = (size_t)-1;

        Buffer()
        {
        }

        Buffer( const QByteArray& _arr )
        :
            QByteArray( _arr )
        {
        }

        Buffer( const QByteArray&& _arr )
        :
            QByteArray( _arr )
        {
        }

        bool empty() const
        {
            return isEmpty();
        }

        void pop_front( size_t count )
        {
            remove( 0, (int)count );
        }

        Buffer substr( size_t pos, size_t count = npos ) const
        {
            return QByteArray::fromRawData(
                data() + pos,
                (int)(count == npos ? size()-pos : count) );
        }

        size_t size() const
        {
            return (size_t)QByteArray::size();
        }
    };
}

#endif  //NX_BUFFER_H
