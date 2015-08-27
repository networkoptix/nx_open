/**********************************************************
* 28 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef NX_BUFFER_H
#define NX_BUFFER_H

#include <string>
#include <stdint.h>

#include <limits>

#include <QtCore/QByteArray>

#ifdef _WIN32
#undef max
#undef min
#endif


namespace nx
{
    /*!
        Some effective buffer is required. Following is desired:\n
            - substr O(1) complexity
            - pop_front, pop_back O(1) complexity
            - concatenation O(1) complexity. This implies readiness for readv and writev system calls
            - buffer should implicit sharing but still minimize atomic operations where they not required

        Currently, using QByteArray, but fully new implementation will be provided in 2.4
    */
    typedef QByteArray Buffer;

    /*!
        Some effective buffer is required. Following is desired:\n
            - all features of \class Buffer
            - QString compartability (with checkups for non ASCII symbols)
     */
    typedef QByteArray String;

    bool operator==( const std::string& left, const nx::String& right );
    bool operator==( const nx::String& left, const std::string& right );
    bool operator!=( const std::string& left, const nx::String& right );
    bool operator!=( const nx::String& left, const std::string& right );
}

#endif  //NX_BUFFER_H
