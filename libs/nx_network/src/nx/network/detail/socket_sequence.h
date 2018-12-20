/**********************************************************
* Nov 12, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_SOCKET_SEQUENCE_H
#define NX_SOCKET_SEQUENCE_H

#include <stdint.h>


#ifdef __arm__
//ISD Jaguar requires kernel update to support 64-bit atomics
typedef int SocketSequenceType;
#else
typedef uint64_t SocketSequenceType;
#endif

#endif  //NX_SOCKET_SEQUENCE_H
