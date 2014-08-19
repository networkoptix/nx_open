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
    /*!
        Some effective buffer is required. Following is desired:\n
            - substr O(1) complexity
            - pop_front, pop_back O(1) complexity
            - concatenation O(1) complexity. This implies readiness for readv and writev system calls
            - buffer should implicit sharing but still minimize atomic operations where they not required

        Currently, using QByteArray, but fully new implementation will be provided in 2.4
    */
    typedef QByteArray Buffer;
}

#endif  //NX_BUFFER_H
